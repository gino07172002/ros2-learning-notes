// dynamic_broadcaster.cpp
// 用 C++ 發送 dynamic TF（會變的座標關係）
//
// 場景：
//   - 機器人在世界中移動 → world → base_link 持續更新
//   - 機械臂關節轉動 → 手臂 link 之間 TF 持續變
//
// 本範例：模擬一個 base_link 在 world 座標系內以圓周運動
//   軌跡：半徑 1m，每秒走 1 弧度（約 6 秒一圈）

#include "rclcpp/rclcpp.hpp"
#include "tf2_ros/transform_broadcaster.h"
#include "geometry_msgs/msg/transform_stamped.hpp"
#include "tf2/LinearMath/Quaternion.h"

#include <cmath>

using namespace std::chrono_literals;

class DynamicTFBroadcaster : public rclcpp::Node
{
public:
    DynamicTFBroadcaster() : Node("dynamic_tf_broadcaster")
    {
        broadcaster_ = std::make_shared<tf2_ros::TransformBroadcaster>(this);
        start_ = now();

        // 50Hz 發送（業界 odom 常見頻率）
        timer_ = create_wall_timer(20ms, [this]() { publish_tf(); });

        RCLCPP_INFO(get_logger(), "Dynamic TF: world → base_link, circular motion");
    }

private:
    void publish_tf()
    {
        double t = (now() - start_).seconds();

        geometry_msgs::msg::TransformStamped tf_msg;
        tf_msg.header.stamp = now();
        tf_msg.header.frame_id = "world";
        tf_msg.child_frame_id = "base_link";

        // 圓周運動：半徑 1m，角速度 1 rad/s
        tf_msg.transform.translation.x = std::cos(t);
        tf_msg.transform.translation.y = std::sin(t);
        tf_msg.transform.translation.z = 0.0;

        // 朝向：沿圓周切線方向（yaw = t + π/2）
        tf2::Quaternion q;
        q.setRPY(0, 0, t + M_PI / 2);
        tf_msg.transform.rotation.x = q.x();
        tf_msg.transform.rotation.y = q.y();
        tf_msg.transform.rotation.z = q.z();
        tf_msg.transform.rotation.w = q.w();

        broadcaster_->sendTransform(tf_msg);
    }

    std::shared_ptr<tf2_ros::TransformBroadcaster> broadcaster_;
    rclcpp::TimerBase::SharedPtr timer_;
    rclcpp::Time start_;
};

int main(int argc, char ** argv)
{
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<DynamicTFBroadcaster>());
    rclcpp::shutdown();
    return 0;
}
