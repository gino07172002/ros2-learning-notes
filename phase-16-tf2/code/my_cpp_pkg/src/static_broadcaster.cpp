// static_broadcaster.cpp
// 用 C++ 發送 static TF（不會變的座標關係）
//
// 場景：
//   - URDF 內已經有大部分 link，但有些動態加上去（例如外掛感測器）
//   - 不想寫 URDF 卻又要發 TF（快速 prototype）
//
// 跟 dynamic broadcaster 差別：static 用 TF_STATIC topic（latched），
// 訂閱者一連上就收到（不需要等下次發送）

#include "rclcpp/rclcpp.hpp"
#include "tf2_ros/static_transform_broadcaster.h"
#include "geometry_msgs/msg/transform_stamped.hpp"

class StaticTFBroadcaster : public rclcpp::Node
{
public:
    StaticTFBroadcaster() : Node("static_tf_broadcaster")
    {
        broadcaster_ = std::make_shared<tf2_ros::StaticTransformBroadcaster>(this);

        // 發送一個固定 TF: world → my_sensor
        // 表示 my_sensor 在 world 座標系的位置
        geometry_msgs::msg::TransformStamped t;
        t.header.stamp = now();
        t.header.frame_id = "world";        // 父
        t.child_frame_id = "my_sensor";     // 子
        t.transform.translation.x = 1.0;
        t.transform.translation.y = 2.0;
        t.transform.translation.z = 0.5;
        // 沒旋轉時 quaternion 是 (0,0,0,1) — 必填，否則 TF 會抱怨
        t.transform.rotation.w = 1.0;

        broadcaster_->sendTransform(t);
        RCLCPP_INFO(get_logger(), "Static TF: world → my_sensor at (1.0, 2.0, 0.5)");
    }

private:
    std::shared_ptr<tf2_ros::StaticTransformBroadcaster> broadcaster_;
};

int main(int argc, char ** argv)
{
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<StaticTFBroadcaster>());
    rclcpp::shutdown();
    return 0;
}
