// qos_subscriber.cpp
// 配合 qos_demo 的 publisher，演示「QoS 不匹配 = 收不到」
//
// 用法（要跟 publisher 的 QoS 對齊才能收到）：
//   ros2 run phase26_pkg qos_subscriber reliable
//   ros2 run phase26_pkg qos_subscriber best_effort
//   ros2 run phase26_pkg qos_subscriber latched

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/int32.hpp"
#include <string>

class QoSSubscriber : public rclcpp::Node
{
public:
    QoSSubscriber(const std::string & mode) : Node("qos_subscriber")
    {
        rclcpp::QoS qos(10);

        if (mode == "reliable") {
            qos.reliable();
        } else if (mode == "best_effort") {
            qos.best_effort();
        } else if (mode == "latched") {
            qos.transient_local().reliable();
        } else {
            qos.reliable();
        }

        RCLCPP_INFO(get_logger(), "Subscriber QoS: %s", mode.c_str());

        sub_ = create_subscription<std_msgs::msg::Int32>(
            "counter", qos,
            [this](const std_msgs::msg::Int32::SharedPtr msg) {
                RCLCPP_INFO(get_logger(), "Received: %d", msg->data);
            });
    }

private:
    rclcpp::Subscription<std_msgs::msg::Int32>::SharedPtr sub_;
};

int main(int argc, char ** argv)
{
    rclcpp::init(argc, argv);
    std::string mode = (argc > 1) ? argv[1] : "default";
    rclcpp::spin(std::make_shared<QoSSubscriber>(mode));
    rclcpp::shutdown();
    return 0;
}
