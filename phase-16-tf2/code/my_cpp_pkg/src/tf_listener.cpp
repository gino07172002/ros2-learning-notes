// tf_listener.cpp
// 用 C++ 訂閱 TF tree 並查詢座標關係
//
// 業界場景：
//   - 機器人在 world 移動，需要把光達點雲（在 lidar frame）轉到 base_link
//   - SLAM 算地圖時需要連續查 odom → base_link 的 TF
//
// 重要 API:
//   - tf2_ros::Buffer 儲存 TF history
//   - tf2_ros::TransformListener 訂閱 /tf 與 /tf_static 並填到 Buffer
//   - buffer.lookupTransform(target, source, time) 查任意時刻的 TF

#include "rclcpp/rclcpp.hpp"
#include "tf2_ros/transform_listener.h"
#include "tf2_ros/buffer.h"
#include "geometry_msgs/msg/transform_stamped.hpp"

using namespace std::chrono_literals;

class TFListenerNode : public rclcpp::Node
{
public:
    TFListenerNode() : Node("tf_listener_node")
    {
        // ⚠️ Buffer 必須給 clock，這是 ROS 2 的常見雷
        tf_buffer_ = std::make_unique<tf2_ros::Buffer>(get_clock());
        tf_listener_ = std::make_shared<tf2_ros::TransformListener>(*tf_buffer_);

        timer_ = create_wall_timer(1s, [this]() { lookup(); });
    }

private:
    void lookup()
    {
        try {
            // tf2::TimePointZero = 「最新可用的」TF（不指定時間）
            auto t = tf_buffer_->lookupTransform(
                "world",         // 目標座標系
                "base_link",     // 來源座標系
                tf2::TimePointZero);

            RCLCPP_INFO(get_logger(),
                "world ← base_link: x=%.2f y=%.2f z=%.2f",
                t.transform.translation.x,
                t.transform.translation.y,
                t.transform.translation.z);
        } catch (const tf2::TransformException & e) {
            // ⚠️ 必接：剛啟動時 TF 還沒到、或時間戳對不上會 throw
            RCLCPP_WARN(get_logger(), "Could not transform: %s", e.what());
        }
    }

    std::unique_ptr<tf2_ros::Buffer> tf_buffer_;
    std::shared_ptr<tf2_ros::TransformListener> tf_listener_;
    rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char ** argv)
{
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<TFListenerNode>());
    rclcpp::shutdown();
    return 0;
}
