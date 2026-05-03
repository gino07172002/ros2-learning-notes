// qos_demo.cpp
// 比較不同 QoS profile 的行為差異
//
// 用法：
//   ros2 run phase26_pkg qos_demo reliable     # Reliability=Reliable
//   ros2 run phase26_pkg qos_demo best_effort  # Reliability=Best Effort
//   ros2 run phase26_pkg qos_demo latched      # Durability=Transient Local（latched）
//   ros2 run phase26_pkg qos_demo deadline     # 設 1Hz deadline，故意慢於它測 callback

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/int32.hpp"
#include <string>

using namespace std::chrono_literals;

class QoSDemo : public rclcpp::Node
{
public:
    QoSDemo(const std::string & mode) : Node("qos_demo")
    {
        rclcpp::QoS qos(10);

        if (mode == "reliable") {
            qos.reliable();
            RCLCPP_INFO(get_logger(), "QoS: Reliable (TCP-like, no loss)");
        } else if (mode == "best_effort") {
            qos.best_effort();
            RCLCPP_INFO(get_logger(), "QoS: Best Effort (UDP-like, may lose)");
        } else if (mode == "latched") {
            // 訂閱者一連上立刻收到最後一個訊息（不用等下次發送）
            qos.transient_local().reliable();
            RCLCPP_INFO(get_logger(), "QoS: Transient Local (latched)");
        } else if (mode == "deadline") {
            // 承諾每 1 秒至少一筆。沒達到觸發 deadline_callback
            qos.deadline(std::chrono::milliseconds(1000));
            RCLCPP_INFO(get_logger(), "QoS: Deadline 1000ms");
        } else {
            qos.reliable();
            RCLCPP_INFO(get_logger(), "QoS: default (reliable)");
        }

        publisher_ = create_publisher<std_msgs::msg::Int32>("counter", qos);

        // 故意每 2 秒發一次（如果 deadline 1s 會 violation）
        const auto period = (mode == "deadline") ? 2s : 500ms;

        timer_ = create_wall_timer(period, [this]() {
            auto msg = std_msgs::msg::Int32();
            msg.data = count_++;
            publisher_->publish(msg);
            RCLCPP_INFO(get_logger(), "Published: %d", msg.data);
        });
    }

private:
    rclcpp::Publisher<std_msgs::msg::Int32>::SharedPtr publisher_;
    rclcpp::TimerBase::SharedPtr timer_;
    int count_ = 0;
};

int main(int argc, char ** argv)
{
    rclcpp::init(argc, argv);
    std::string mode = (argc > 1) ? argv[1] : "default";
    rclcpp::spin(std::make_shared<QoSDemo>(mode));
    rclcpp::shutdown();
    return 0;
}
