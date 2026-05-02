#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/twist.hpp"
#include <chrono>

using namespace std::chrono_literals;

class AutoDriveNode : public rclcpp::Node
{
public:
    AutoDriveNode() : Node("auto_drive_node")
    {
        publisher_ = this->create_publisher<geometry_msgs::msg::Twist>("cmd_vel", 10);
        timer_ = this->create_wall_timer(
            500ms, std::bind(&AutoDriveNode::timer_callback, this));
        start_time_ = this->now();
    }

private:
    void timer_callback()
    {
        auto msg = geometry_msgs::msg::Twist();
        auto elapsed = this->now() - start_time_;

        if (elapsed.seconds() < 3.0) {
            msg.linear.x = 0.2;
            msg.angular.z = 0.0;
        } else {
            msg.linear.x = 0.0;
            msg.angular.z = 0.0;
        }
        publisher_->publish(msg);
    }

    rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr publisher_;
    rclcpp::TimerBase::SharedPtr timer_;
    rclcpp::Time start_time_;
};

int main(int argc, char * argv[])
{
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<AutoDriveNode>());
    rclcpp::shutdown();
    return 0;
}
