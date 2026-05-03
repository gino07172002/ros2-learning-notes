// composable_subscriber.cpp
// 配合 composable_publisher 的訂閱端

#include "rclcpp/rclcpp.hpp"
#include "rclcpp_components/register_node_macro.hpp"
#include "std_msgs/msg/string.hpp"

namespace phase09_components
{

class ComposableSubscriber : public rclcpp::Node
{
public:
    explicit ComposableSubscriber(const rclcpp::NodeOptions & options)
    : Node("composable_subscriber", options)
    {
        subscription_ = create_subscription<std_msgs::msg::String>(
            "chat", 10,
            [this](const std_msgs::msg::String::SharedPtr msg) {
                RCLCPP_INFO(get_logger(), "Received: %s", msg->data.c_str());
            });
    }

private:
    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr subscription_;
};

}  // namespace phase09_components

RCLCPP_COMPONENTS_REGISTER_NODE(phase09_components::ComposableSubscriber)
