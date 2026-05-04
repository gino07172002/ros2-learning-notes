// Advanced/perception/04: PCL pipeline
//   訂 PointCloud2 → VoxelGrid → 移除地面(RANSAC) → Euclidean clustering → 發 Detection3DArray
//
// 跑法:
//   ros2 run my_pcl_demo cluster_extractor \
//     --ros-args -r cloud_in:=/intel_realsense_r200_depth/points

#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/point_cloud2.hpp>
#include <vision_msgs/msg/detection3_d_array.hpp>

#include <pcl_conversions/pcl_conversions.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/filters/voxel_grid.h>
#include <pcl/filters/extract_indices.h>
#include <pcl/segmentation/sac_segmentation.h>
#include <pcl/segmentation/extract_clusters.h>
#include <pcl/search/kdtree.h>
#include <pcl/common/common.h>

using PointT = pcl::PointXYZ;
using CloudT = pcl::PointCloud<PointT>;

class ClusterExtractor : public rclcpp::Node
{
public:
    ClusterExtractor() : Node("cluster_extractor")
    {
        // 大訊息一律 SensorDataQoS
        sub_ = create_subscription<sensor_msgs::msg::PointCloud2>(
            "cloud_in", rclcpp::SensorDataQoS(),
            std::bind(&ClusterExtractor::on_cloud, this, std::placeholders::_1));

        pub_ = create_publisher<vision_msgs::msg::Detection3DArray>(
            "detections_3d", 10);

        RCLCPP_INFO(get_logger(),
            "cluster_extractor ready: subscribing 'cloud_in', publishing 'detections_3d'");
    }

private:
    void on_cloud(const sensor_msgs::msg::PointCloud2::SharedPtr msg)
    {
        // 1. ROS → PCL
        CloudT::Ptr cloud(new CloudT);
        pcl::fromROSMsg(*msg, *cloud);
        if (cloud->empty()) return;

        // 2. VoxelGrid 降採樣(5cm 解析度,點數降約 10 倍)
        CloudT::Ptr cloud_ds(new CloudT);
        pcl::VoxelGrid<PointT> vg;
        vg.setInputCloud(cloud);
        vg.setLeafSize(0.05f, 0.05f, 0.05f);
        vg.filter(*cloud_ds);

        // 3. RANSAC 找地面 → 移除
        pcl::SACSegmentation<PointT> seg;
        seg.setOptimizeCoefficients(true);
        seg.setModelType(pcl::SACMODEL_PLANE);
        seg.setMethodType(pcl::SAC_RANSAC);
        seg.setDistanceThreshold(0.05);
        seg.setMaxIterations(100);

        pcl::PointIndices::Ptr inliers(new pcl::PointIndices);
        pcl::ModelCoefficients::Ptr coefs(new pcl::ModelCoefficients);
        seg.setInputCloud(cloud_ds);
        seg.segment(*inliers, *coefs);

        if (inliers->indices.empty()) {
            RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 2000,
                "No ground plane found in cloud (%zu points)", cloud_ds->size());
            return;
        }

        CloudT::Ptr cloud_no_ground(new CloudT);
        pcl::ExtractIndices<PointT> extract;
        extract.setInputCloud(cloud_ds);
        extract.setIndices(inliers);
        extract.setNegative(true);
        extract.filter(*cloud_no_ground);

        // 4. Euclidean clustering
        pcl::search::KdTree<PointT>::Ptr tree(new pcl::search::KdTree<PointT>);
        tree->setInputCloud(cloud_no_ground);

        std::vector<pcl::PointIndices> cluster_indices;
        pcl::EuclideanClusterExtraction<PointT> ec;
        ec.setClusterTolerance(0.10);   // 10cm 以內視為同一物件
        ec.setMinClusterSize(20);
        ec.setMaxClusterSize(5000);
        ec.setSearchMethod(tree);
        ec.setInputCloud(cloud_no_ground);
        ec.extract(cluster_indices);

        // 5. 算 bbox + 發 Detection3DArray
        vision_msgs::msg::Detection3DArray detections;
        detections.header = msg->header;

        for (const auto & ci : cluster_indices) {
            CloudT::Ptr cluster(new CloudT);
            for (int idx : ci.indices) {
                cluster->push_back((*cloud_no_ground)[idx]);
            }

            PointT min_pt, max_pt;
            pcl::getMinMax3D(*cluster, min_pt, max_pt);

            vision_msgs::msg::Detection3D det;
            det.header = msg->header;
            det.bbox.center.position.x = (min_pt.x + max_pt.x) / 2.0;
            det.bbox.center.position.y = (min_pt.y + max_pt.y) / 2.0;
            det.bbox.center.position.z = (min_pt.z + max_pt.z) / 2.0;
            det.bbox.center.orientation.w = 1.0;
            det.bbox.size.x = max_pt.x - min_pt.x;
            det.bbox.size.y = max_pt.y - min_pt.y;
            det.bbox.size.z = max_pt.z - min_pt.z;
            detections.detections.push_back(det);
        }

        RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), 2000,
            "Found %zu clusters from %zu points (down from %zu raw)",
            cluster_indices.size(), cloud_no_ground->size(), cloud->size());

        pub_->publish(detections);
    }

    rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::SharedPtr sub_;
    rclcpp::Publisher<vision_msgs::msg::Detection3DArray>::SharedPtr pub_;
};

int main(int argc, char * argv[])
{
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<ClusterExtractor>());
    rclcpp::shutdown();
    return 0;
}
