// comparator.cpp — Phase 20A
//
// 訂閱 /wheel/odometry 和 /odometry/filtered,並用第一性原理算「真值」
// (我們知道車是定速圓周運動,所以從啟動時刻可推真位置)
// 每秒打印一次三者的 x/y,給 README demo 用。
//
// 真值 (t 秒後位置):
//   yaw_true = w * t
//   x_true = (v/w) * sin(yaw_true)
//   y_true = (v/w) * (1 - cos(yaw_true))

#include <chrono>
#include <cmath>
#include <rclcpp/rclcpp.hpp>
#include <nav_msgs/msg/odometry.hpp>
#include <tf2/utils.h>
#include <tf2_geometry_msgs/tf2_geometry_msgs.hpp>

class Comparator : public rclcpp::Node
{
public:
  Comparator() : Node("ekf_comparator")
  {
    declare_parameter("true_linear_x", 0.5);
    declare_parameter("true_angular_z", 0.3);

    wheel_sub_ = create_subscription<nav_msgs::msg::Odometry>(
      "wheel/odometry", 10,
      [this](nav_msgs::msg::Odometry::SharedPtr msg) { wheel_ = *msg; have_wheel_ = true; });

    ekf_sub_ = create_subscription<nav_msgs::msg::Odometry>(
      "odometry/filtered", 10,
      [this](nav_msgs::msg::Odometry::SharedPtr msg) { ekf_ = *msg; have_ekf_ = true; });

    start_ = now();
    timer_ = create_wall_timer(std::chrono::seconds(1),
                               std::bind(&Comparator::tick, this));
    RCLCPP_INFO(get_logger(), "comparator up");
  }

private:
  void tick()
  {
    if (!have_wheel_ || !have_ekf_) {
      RCLCPP_INFO(get_logger(), "waiting for first samples...");
      return;
    }
    const double v = get_parameter("true_linear_x").as_double();
    const double w = get_parameter("true_angular_z").as_double();
    const double t = (now() - start_).seconds();
    const double yaw_t = w * t;
    const double x_t = (v / w) * std::sin(yaw_t);
    const double y_t = (v / w) * (1.0 - std::cos(yaw_t));

    auto wheel_yaw = tf2::getYaw(wheel_.pose.pose.orientation);
    auto ekf_yaw   = tf2::getYaw(ekf_.pose.pose.orientation);

    RCLCPP_INFO(get_logger(),
      "t=%5.1fs | TRUE  x=%6.3f y=%6.3f yaw=%6.3f", t, x_t, y_t, yaw_t);
    RCLCPP_INFO(get_logger(),
      "         | WHEEL x=%6.3f y=%6.3f yaw=%6.3f  (Δ=%5.3f)",
      wheel_.pose.pose.position.x, wheel_.pose.pose.position.y, wheel_yaw,
      std::hypot(wheel_.pose.pose.position.x - x_t,
                 wheel_.pose.pose.position.y - y_t));
    RCLCPP_INFO(get_logger(),
      "         | EKF   x=%6.3f y=%6.3f yaw=%6.3f  (Δ=%5.3f)",
      ekf_.pose.pose.position.x, ekf_.pose.pose.position.y, ekf_yaw,
      std::hypot(ekf_.pose.pose.position.x - x_t,
                 ekf_.pose.pose.position.y - y_t));
  }

  rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr wheel_sub_;
  rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr ekf_sub_;
  rclcpp::TimerBase::SharedPtr timer_;
  rclcpp::Time start_;
  nav_msgs::msg::Odometry wheel_, ekf_;
  bool have_wheel_ = false, have_ekf_ = false;
};

int main(int argc, char **argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<Comparator>());
  rclcpp::shutdown();
  return 0;
}
