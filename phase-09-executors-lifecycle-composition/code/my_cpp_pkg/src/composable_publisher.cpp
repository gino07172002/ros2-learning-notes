// composable_publisher.cpp
// 可組合的 Publisher Node — 用 rclcpp_components 註冊
//
// 一般 Node 跑起來 = 一個 process 一個 Node
// 可組合 Node 跑起來 = 一個 process 多個 Node（節省 IPC 開銷）
//
// 業界用途：Nav2 / MoveIt 把 10+ 個 Node 組進 1 個 process，效能比分開跑好很多
// 因為同 process 的 Node 之間用 intra-process communication (零拷貝)

#include "rclcpp/rclcpp.hpp"
#include "rclcpp_components/register_node_macro.hpp"
#include "std_msgs/msg/string.hpp"

using namespace std::chrono_literals;

namespace phase09_components
{

class ComposablePublisher : public rclcpp::Node
{
public:
    // ⚠️ 可組合 Node 的建構子必須接受 NodeOptions 參數
    explicit ComposablePublisher(const rclcpp::NodeOptions & options)
    : Node("composable_publisher", options)
    {
        publisher_ = create_publisher<std_msgs::msg::String>("chat", 10);
        timer_ = create_wall_timer(1s, [this]() {
            auto msg = std_msgs::msg::String();
            msg.data = "Hello from publisher #" + std::to_string(count_++);
            RCLCPP_INFO(get_logger(), "Publishing: %s", msg.data.c_str());
            publisher_->publish(msg);
        });
    }

private:
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr publisher_;
    rclcpp::TimerBase::SharedPtr timer_;
    int count_ = 0;
};

}  // namespace phase09_components

// ⚠️ 關鍵：用巨集向 rclcpp_components 註冊本 class
// 註冊後 ros2 component load 才找得到
RCLCPP_COMPONENTS_REGISTER_NODE(phase09_components::ComposablePublisher)
