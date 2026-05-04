// Advanced/perception/01: 訂 sensor_msgs/Image,跑 Canny edge detection,發回處理過的 Image
//
// 跑法:
//   ros2 run my_camera_demo edge_detector \
//     --ros-args -r image_in:=/camera/image_raw -r image_out:=/camera/edges

#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/image.hpp>
#include <cv_bridge/cv_bridge.h>
#include <opencv2/opencv.hpp>

class EdgeDetector : public rclcpp::Node
{
public:
    EdgeDetector() : Node("edge_detector")
    {
        // ✅ Image 訊息必須用 SensorDataQoS(BestEffort + KeepLast 5)
        // 用預設 Reliable 會卡住整個 callback chain(雷 4)
        sub_ = create_subscription<sensor_msgs::msg::Image>(
            "image_in", rclcpp::SensorDataQoS(),
            std::bind(&EdgeDetector::on_image, this, std::placeholders::_1));

        pub_ = create_publisher<sensor_msgs::msg::Image>(
            "image_out", rclcpp::SensorDataQoS());

        RCLCPP_INFO(get_logger(),
            "edge_detector ready: subscribing 'image_in', publishing 'image_out'");
    }

private:
    void on_image(const sensor_msgs::msg::Image::SharedPtr msg)
    {
        // 1. ROS Image → cv::Mat (deep copy)
        // toCvCopy 會自動處理 RGB/BGR 轉換,不要硬寫 "bgr8" 字串
        cv_bridge::CvImagePtr cv_ptr;
        try {
            cv_ptr = cv_bridge::toCvCopy(msg, sensor_msgs::image_encodings::BGR8);
        } catch (cv_bridge::Exception & e) {
            RCLCPP_ERROR(get_logger(), "cv_bridge error: %s", e.what());
            return;
        }

        // 2. 處理 — 灰階 → Canny edge detection
        cv::Mat gray, edges;
        cv::cvtColor(cv_ptr->image, gray, cv::COLOR_BGR2GRAY);
        cv::Canny(gray, edges, 100, 200);

        // 3. cv::Mat → ROS Image,**保留原 header**(時間戳 + frame_id)
        // 下游做 visual servoing / TF 對齊需要這個
        auto out = cv_bridge::CvImage(
            msg->header,
            sensor_msgs::image_encodings::MONO8,
            edges).toImageMsg();

        pub_->publish(*out);

        // 限頻 log,別 spam
        RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), 5000,
            "Processed frame: %dx%d", cv_ptr->image.cols, cv_ptr->image.rows);
    }

    rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr sub_;
    rclcpp::Publisher<sensor_msgs::msg::Image>::SharedPtr pub_;
};

int main(int argc, char * argv[])
{
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<EdgeDetector>());
    rclcpp::shutdown();
    return 0;
}
