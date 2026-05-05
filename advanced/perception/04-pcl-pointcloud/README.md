# 04. PCL — 3D 點雲處理

> 從 Phase 03 訂 LiDAR `PointCloud2` 自然延伸到「**用 PCL 過濾 + 分割 + 抓出物件**」。3D 點雲是自駕車 / 倉儲機器人 / 高精度 SLAM 標配。

**學完你會**:
- 訂 `sensor_msgs/PointCloud2` + 用 `pcl_conversions::fromROSMsg` 轉成 `pcl::PointCloud<pcl::PointXYZ>`
- **Voxel Grid 降採樣**:100k 點 → 10k 點(處理速度快 10 倍)
- **平面分割**:RANSAC 找出地面平面 → 移除地面點
- **歐式分群**(Euclidean clustering):把剩下的點分成獨立物件
- 發 `vision_msgs/Detection3DArray` 給下游(整合 Phase 08 整合情境內的 DetectedObjects)

**前置**:
- [Phase 03 Subscriber + 光達避障](../../../phase-03-subscriber-lidar-brake/) — 訂 PointCloud2 觀念
- [01. Camera + cv_bridge](../01-camera-cv-bridge/) — 同套 perception 處理流程

**產出**:[`code/my_pcl_demo/`](code/my_pcl_demo/) — 訂 PointCloud2 + PCL 處理 + 發 Detection3DArray

**環境**:☁️ TheConstructSim(turtlebot3_world 自帶 LiDAR PointCloud2)/ 💻 本機 WSL2

---

## 📍 為什麼點雲處理要單獨一章

PointCloud2 跟 Image 都是 sensor 資料,**處理 pipeline 完全不同**:

| 步驟 | Image(OpenCV) | PointCloud(PCL) |
|------|---------------|-------------------|
| 降採樣 | `cv::resize` | `VoxelGrid` filter |
| 邊緣偵測 | `cv::Canny` | 平面 / 圓柱分割(RANSAC) |
| 物件偵測 | `cv::findContours` 或 YOLO | Euclidean clustering |
| 多 sensor 融合 | 拼接 → cv::Mat 大圖 | `pcl::concatenate` |

業界 mobile robot 對 LiDAR 點雲的標準 pipeline:

```
原始 PointCloud2(100k 點 / scan)
  ↓ Voxel Grid(降採樣到 5cm 解析度)
PointCloud(10k 點)
  ↓ Pass-through filter(只留高度 0.05m–2m,刪掉天花板地板邊角)
PointCloud(8k 點)
  ↓ RANSAC plane segmentation(找出地面)
PointCloud(地面 5k) + PointCloud(非地面 3k)
  ↓ Euclidean clustering on 非地面
N 個獨立物件 cluster(每個一個 PointCloud)
  ↓ 算每個 cluster 的 bbox
Detection3DArray(N 個 BoundingBox3D)
```

本章把這整個 pipeline 寫成一個 Node。

---

## ⚠️ 關鍵知識:`sensor_msgs/PointCloud2` ↔ `pcl::PointCloud` 互轉

PCL 用自己的型別 `pcl::PointCloud<pcl::PointXYZ>`,不是 ROS 訊息。中間需要 `pcl_conversions` 橋接:

```cpp
#include <pcl_conversions/pcl_conversions.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <sensor_msgs/msg/point_cloud2.hpp>

void on_cloud(const sensor_msgs::msg::PointCloud2::SharedPtr msg)
{
    // ROS 訊息 → PCL 物件
    pcl::PointCloud<pcl::PointXYZ>::Ptr cloud(new pcl::PointCloud<pcl::PointXYZ>);
    pcl::fromROSMsg(*msg, *cloud);

    // ... 處理 ...

    // PCL 物件 → ROS 訊息
    sensor_msgs::msg::PointCloud2 out;
    pcl::toROSMsg(*processed_cloud, out);
    out.header = msg->header;     // ← 重要:保留 frame_id 跟時間戳
    pub_->publish(out);
}
```

> ⚠️ **新手雷**:`pcl::fromROSMsg` 在某些舊版 PCL 對 `is_dense` 欄位很挑剔(雷 5)。

---

## 💻 步驟 1:寫 PCL 處理 Node

完整見 [`code/my_pcl_demo/src/cluster_extractor.cpp`](code/my_pcl_demo/src/cluster_extractor.cpp)。

```cpp
#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/point_cloud2.hpp>
#include <vision_msgs/msg/detection3_d_array.hpp>

#include <pcl_conversions/pcl_conversions.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/filters/voxel_grid.h>
#include <pcl/segmentation/sac_segmentation.h>
#include <pcl/filters/extract_indices.h>
#include <pcl/segmentation/extract_clusters.h>
#include <pcl/common/common.h>
#include <pcl/search/kdtree.h>

using PointT = pcl::PointXYZ;
using CloudT = pcl::PointCloud<PointT>;

class ClusterExtractor : public rclcpp::Node
{
public:
    ClusterExtractor() : Node("cluster_extractor")
    {
        sub_ = create_subscription<sensor_msgs::msg::PointCloud2>(
            "cloud_in", rclcpp::SensorDataQoS(),
            std::bind(&ClusterExtractor::on_cloud, this, std::placeholders::_1));

        pub_ = create_publisher<vision_msgs::msg::Detection3DArray>(
            "detections_3d", 10);
    }

private:
    void on_cloud(const sensor_msgs::msg::PointCloud2::SharedPtr msg)
    {
        // 1. ROS → PCL
        CloudT::Ptr cloud(new CloudT);
        pcl::fromROSMsg(*msg, *cloud);
        if (cloud->empty()) return;

        // 2. Voxel Grid 降採樣(5cm 解析度)
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
        seg.setDistanceThreshold(0.05);   // 5cm 厚度的地面
        seg.setMaxIterations(100);

        pcl::PointIndices::Ptr inliers(new pcl::PointIndices);
        pcl::ModelCoefficients::Ptr coefs(new pcl::ModelCoefficients);
        seg.setInputCloud(cloud_ds);
        seg.segment(*inliers, *coefs);

        if (inliers->indices.empty()) {
            RCLCPP_WARN(get_logger(), "No ground plane found");
            return;
        }

        CloudT::Ptr cloud_no_ground(new CloudT);
        pcl::ExtractIndices<PointT> extract;
        extract.setInputCloud(cloud_ds);
        extract.setIndices(inliers);
        extract.setNegative(true);   // 取「不是地面」的點
        extract.filter(*cloud_no_ground);

        // 4. Euclidean clustering 把剩下的點分成獨立物件
        pcl::search::KdTree<PointT>::Ptr tree(new pcl::search::KdTree<PointT>);
        tree->setInputCloud(cloud_no_ground);

        std::vector<pcl::PointIndices> cluster_indices;
        pcl::EuclideanClusterExtraction<PointT> ec;
        ec.setClusterTolerance(0.10);  // 10cm 以內視為同一物件
        ec.setMinClusterSize(20);      // 至少 20 點才算一個物件(過濾雜訊)
        ec.setMaxClusterSize(5000);
        ec.setSearchMethod(tree);
        ec.setInputCloud(cloud_no_ground);
        ec.extract(cluster_indices);

        // 5. 算每個 cluster 的 BoundingBox + 發 Detection3DArray
        vision_msgs::msg::Detection3DArray detections;
        detections.header = msg->header;

        for (const auto & ci : cluster_indices) {
            CloudT::Ptr cluster(new CloudT);
            for (int idx : ci.indices) {
                cluster->push_back((*cloud_no_ground)[idx]);
            }

            // 算 bbox
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
            "Found %zu clusters from %zu points", cluster_indices.size(), cloud->size());

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
```

---

## ⚙️ 步驟 2:CMakeLists.txt 雷區

```cmake
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(vision_msgs REQUIRED)
find_package(pcl_conversions REQUIRED)
find_package(PCL REQUIRED COMPONENTS common filters segmentation search)

add_executable(cluster_extractor src/cluster_extractor.cpp)

ament_target_dependencies(cluster_extractor
    rclcpp sensor_msgs vision_msgs pcl_conversions)

# PCL 是 system-level,跟 OpenCV 一樣要 target_link_libraries
target_include_directories(cluster_extractor PUBLIC ${PCL_INCLUDE_DIRS})
target_link_libraries(cluster_extractor ${PCL_LIBRARIES})
```

> ⚠️ 同 perception/01,**PCL 也是 system-level 套件**,不能放 ament_target_dependencies。

---

## 🚀 步驟 3:跑 Demo

### ☁️ TheConstructSim 步驟

```bash
sudo apt install ros-humble-pcl-conversions ros-humble-pcl-ros \
                 ros-humble-vision-msgs libpcl-dev
# (雲端 ROSject 通常已預裝 PCL,vision_msgs 可能要裝)

cd ~/ros2_ws/src
git clone https://github.com/gino07172002/ros2-learning-notes.git
cp -r ros2-learning-notes/advanced/perception/04-pcl-pointcloud/code/my_pcl_demo .

cd ~/ros2_ws
colcon build --packages-select my_pcl_demo
source install/setup.bash

# 啟動 turtlebot3_world(自帶 LDS LiDAR,但 LDS 是 2D LaserScan,
# 這章要 PointCloud2,需要轉換 — 用 turtlebot3_waffle 帶 RealSense 才有 3D)
export TURTLEBOT3_MODEL=waffle_pi    # waffle_pi 帶 RealSense 深度相機
ros2 launch turtlebot3_gazebo turtlebot3_world.launch.py

# 另開 terminal 跑我們的 cluster_extractor
ros2 run my_pcl_demo cluster_extractor \
  --ros-args -r cloud_in:=/intel_realsense_r200_depth/points
```

### 💻 本機 WSL2

同上。但 turtlebot3_waffle_pi 在 WSL2 比較吃資源,headless 跑比較順。

---

## 🎯 步驟 4:驗證

```bash
# 1. 確認 input PointCloud2 在
ros2 topic hz /intel_realsense_r200_depth/points
# 預期:30 Hz 上下

# 2. 確認 detections 在發
ros2 topic echo /detections_3d --once
# 預期看到:
#   detections:
#     - bbox:
#         center: {position: {...}, orientation: {...}}
#         size: {x: 0.3, y: 0.3, z: 0.5}
#     - bbox: ...

# 3. log 看 cluster 數
# console 應該每 2 秒印一次:
#   "Found 3 clusters from 8742 points"
```

### 視覺化(用 Foxglove)

```bash
ros2 run foxglove_bridge foxglove_bridge
# 瀏覽器:Foxglove → ws://localhost:8765
# 加 3D panel,顯示 PointCloud2 + Detection3DArray
# 應該看到:點雲 + 每個物件外圍的方框
```

---

## 🐛 常見雷

### 雷 1:`pcl::fromROSMsg` runtime crash 或卡住

**症狀**:Node 啟動沒問題,但收到 PointCloud2 訊息後 segfault 或卡 100% CPU。

**可能原因**:
1. **`is_dense=false` + PCL 舊版**:不能處理 NaN 點
2. **point_step 跟 fields 對不上**:Gazebo 發的 PointCloud2 有 RGB / intensity 但你 cast 成 PointXYZ

**解**:
1. 升級 PCL 到 1.12+:`sudo apt install libpcl-dev`(Humble 預設 1.12)
2. 處理前先用 PassThrough filter 移除 NaN:
```cpp
std::vector<int> indices;
pcl::removeNaNFromPointCloud(*cloud, *cloud, indices);
```

### 雷 2:RANSAC 找不到地面 / 找錯地面

**症狀**:`No ground plane found` 一直 spam,或地面分割結果包含牆面。

**原因**:
- LiDAR 沒對齊地面(裝歪了 / TF 沒對)
- `setDistanceThreshold` 太小(地面點不平整)

**解**:
- 先做 PassThrough filter 限制高度範圍(`setLimits(-0.5, 0.5)` 先篩掉天花板)
- distanceThreshold 從 0.02 → 0.05 → 0.10 試
- **進階**:用 `pcl::SACMODEL_PERPENDICULAR_PLANE` 限制法向量平行 z 軸

### 雷 3:Cluster 數爆炸(800+ 個 cluster)

**症狀**:console 印 `Found 847 clusters`。

**原因**:`setClusterTolerance(0.05)` 太小,每個物件被切成上百小塊;或 voxel size 太小。

**解**:
- `setClusterTolerance` 拉大到 0.10–0.20m(物件之間至少多遠才算分開)
- `setMinClusterSize` 拉大到 100+(過濾雜訊)
- voxel size 拉到 10cm,點數先少一半

### 雷 4:處理速度跟不上(input 30Hz、output 5Hz)

**症狀**:`hz /detections_3d` 顯示 5 Hz,慢於 input 30 Hz。

**原因**:VoxelGrid + RANSAC + Clustering 全套跑下來 200ms 一次,**單 thread CPU 跟不上**。

**解**:
- voxel size 從 0.05 改 0.10(點數降 8 倍,速度快 5 倍)
- RANSAC `setMaxIterations(50)` 降迭代
- 用 MultiThreadedExecutor + Reentrant CallbackGroup(Phase 09)分散 callback
- 進階:用 GPU 版 PCL(NVIDIA Isaac ROS pcl_apps)

### 雷 5:`is_dense` 欄位不對 PCL 拒絕

**症狀**:`pcl::fromROSMsg` 報
```
[pcl::PCLBase::deinitCompute] Invalid input!
```

**原因**:Gazebo 老版本發的 PointCloud2 沒設 `is_dense`,PCL 嚴格模式拒絕。

**解**:fromROSMsg 之後手動設:
```cpp
cloud->is_dense = false;   // 或 true,看你資料
```

### 雷 6:Detection3DArray 訊息找不到

**症狀**:`vision_msgs/Detection3DArray` 找不到 .hpp。

**原因**:沒裝 `vision_msgs`。

**解**:`sudo apt install ros-humble-vision-msgs`

---

## 🎯 學到的關鍵概念

- **PCL pipeline 標準步驟**:VoxelGrid → PassThrough → RANSAC plane → Euclidean clustering → bbox
- **`pcl_conversions` 是 PCL ↔ ROS 的橋接**(類似 cv_bridge 對 OpenCV)
- **Voxel size 是性能 / 精度的 tradeoff**:小=精度高但慢,大=快但粗略
- **PCL 跟 OpenCV 一樣是 system-level 套件**,CMake 用 target_link_libraries 不是 ament_target_dependencies
- **記得保留 header**:processed cloud / detections 都要帶原 `msg->header`,下游 TF 對齊靠這個

---

## 🌟 進階挑戰

1. **加 RANSAC 圓柱偵測**:`pcl::SACMODEL_CYLINDER` 找出畫面內的柱子(對倉庫機器人很有用)
2. **多 frame 累積點雲**:用 `pcl::registration` ICP 把連續 10 幀點雲拼起來,降噪用
3. **分類 cluster**:每個 cluster 算 PCA 主軸,長條形=人 / 矩形=方塊 / 接近圓=柱子
4. **接 Phase 30 BT**:把 Detection3D 餵給 BT 的 ConditionNode,Nav2 看到障礙物自動避

---

## 下一步

- 整合 perception 整套到一台機器人:相機(01)+ AprilTag(02)+ YOLO(03)+ PCL(04)
- 接 Track A:把 Detection3DArray 接到 Nav2 的 obstacle_layer

---

---

## ⏸ 驗證前 audit checklist(留給跑驗證的人)

PCL 是**最容易踩 build 雷**的領域(版本差異、API 改名、CMake 設定多)。跑前先檢:

- [ ] **PCL 版本 vs Humble**:Humble 預設 libpcl-dev 1.12,但某些 API 在 1.13/1.14 改了。例:`pcl::SACSegmentation::setOptimizeCoefficients(true)` 在新版本可能 deprecate
- [ ] **`PCL_INCLUDE_DIRS` vs `PCL::PCL` target**:CMake 寫 `${PCL_INCLUDE_DIRS}` 跟 `${PCL_LIBRARIES}` 在新 CMake 可能要改 `PCL::common PCL::filters` 之類的 target name
- [ ] **`pcl_conversions` 的 include**:`<pcl_conversions/pcl_conversions.h>` 在 Humble 確認還在(rolling 已改 `.hpp`)
- [ ] **`vision_msgs/msg/detection3_d_array.hpp`**:消息名 snake_case 轉換 — `Detection3DArray` → `detection3_d_array`(注意 `3_d` 中間有底線),拼錯就 include 找不到
- [ ] **TURTLEBOT3_MODEL=waffle_pi 的 RealSense topic**:本章假設 `/intel_realsense_r200_depth/points`,但實際 turtlebot3 SDF 可能用 `/camera/depth/points` 或 `/camera/depth/color/points`,跑 `ros2 topic list | grep -i depth` 確認
- [ ] **`is_dense` 雷(雷 5)**:程式內沒處理 NaN,實機 PointCloud2 多半 `is_dense=false`,要先 `removeNaNFromPointCloud` 再 fromROSMsg
- [ ] **CPU 跟得上嗎**:VoxelGrid + RANSAC + clustering 全套在 30Hz × 100k 點下 CPU 可能跟不上(雷 4),先量單幀處理時間

跑通後升 ✅,實際 PCL API / CMake 修正寫進「常見雷」。

---

> **驗證狀態**:⏸ 純文字草稿(2026-05-05) — code 結構照 PCL 官方 tutorial + Phase 03 PointCloud 訂閱模式。**PCL 是最容易踩 build 雷的領域**,需要驗證時很可能修 CMake / API name。雲端 / WSL 實際驗證後升 ✅。
