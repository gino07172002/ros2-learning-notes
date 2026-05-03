// auto_navigator.cpp — Capstone A
//
// 啟動 30s 後自動 send 兩個導航 goal 給 Nav2 的 /navigate_to_pose action,
// 並訂閱 feedback 印出 distance_remaining
//
// 用 action client 的標準寫法,演示 Capstone 級別的 Nav2 整合
// (前面 Phase 13 學的 action client 知識直接套用)

#include <chrono>
#include <memory>
#include <vector>

#include "rclcpp/rclcpp.hpp"
#include "rclcpp_action/rclcpp_action.hpp"
#include "nav2_msgs/action/navigate_to_pose.hpp"
#include "geometry_msgs/msg/pose_with_covariance_stamped.hpp"

using namespace std::chrono_literals;
using NavigateToPose = nav2_msgs::action::NavigateToPose;

class AutoNavigator : public rclcpp::Node
{
public:
  AutoNavigator() : Node("auto_navigator"), goal_index_(0)
  {
    // 路線:三個 waypoint 連續導航
    waypoints_.push_back({1.0, 0.0});
    waypoints_.push_back({1.0, 1.0});
    waypoints_.push_back({0.0, 0.0});

    initialpose_pub_ = create_publisher<geometry_msgs::msg::PoseWithCovarianceStamped>(
      "/initialpose", 10);

    nav_client_ = rclcpp_action::create_client<NavigateToPose>(this, "navigate_to_pose");

    // 階段一:啟動 5s 後送 initial pose(amcl 才會收斂)
    initialpose_timer_ = create_wall_timer(5s, [this]() {
      if (initialpose_sent_) return;
      sendInitialPose();
      initialpose_sent_ = true;
    });

    // 階段二:啟動 25s 後開始送 goal(等 nav2 fully active)
    start_timer_ = create_wall_timer(25s, [this]() {
      if (started_) return;
      started_ = true;
      sendNextGoal();
    });
  }

private:
  void sendInitialPose()
  {
    geometry_msgs::msg::PoseWithCovarianceStamped p;
    p.header.frame_id = "map";
    p.header.stamp = now();
    p.pose.pose.position.x = 0.0;
    p.pose.pose.position.y = 0.0;
    p.pose.pose.orientation.w = 1.0;
    // amcl 預設 init covariance:0.5/0.5/(pi/12)^2
    p.pose.covariance[0] = 0.25;
    p.pose.covariance[7] = 0.25;
    p.pose.covariance[35] = 0.068;
    initialpose_pub_->publish(p);
    RCLCPP_INFO(get_logger(), "[Capstone A] sent /initialpose (0,0)");
  }

  void sendNextGoal()
  {
    if (goal_index_ >= waypoints_.size()) {
      RCLCPP_INFO(get_logger(), "[Capstone A] All %zu waypoints done!", waypoints_.size());
      return;
    }

    if (!nav_client_->wait_for_action_server(5s)) {
      RCLCPP_ERROR(get_logger(),
        "[Capstone A] /navigate_to_pose action server not available — Nav2 起來了嗎?");
      return;
    }

    auto [x, y] = waypoints_[goal_index_];

    NavigateToPose::Goal goal;
    goal.pose.header.frame_id = "map";
    goal.pose.header.stamp = now();
    goal.pose.pose.position.x = x;
    goal.pose.pose.position.y = y;
    goal.pose.pose.orientation.w = 1.0;

    rclcpp_action::Client<NavigateToPose>::SendGoalOptions opts;

    opts.feedback_callback =
      [this](auto, const std::shared_ptr<const NavigateToPose::Feedback> fb) {
        RCLCPP_INFO(get_logger(),
          "[Capstone A] goal %zu | distance_remaining=%.2f m  navigation_time=%.1fs",
          goal_index_, fb->distance_remaining,
          rclcpp::Duration(fb->navigation_time).seconds());
      };

    opts.result_callback =
      [this](const auto & wrapped) {
        if (wrapped.code == rclcpp_action::ResultCode::SUCCEEDED) {
          RCLCPP_INFO(get_logger(),
            "[Capstone A] ✅ goal %zu reached", goal_index_);
        } else {
          RCLCPP_WARN(get_logger(),
            "[Capstone A] ❌ goal %zu failed (code %d)",
            goal_index_, static_cast<int>(wrapped.code));
        }
        goal_index_++;
        // 下一個 goal 過 5 秒再送
        next_goal_timer_ = create_wall_timer(5s, [this]() {
          next_goal_timer_->cancel();
          sendNextGoal();
        });
      };

    RCLCPP_INFO(get_logger(),
      "[Capstone A] sending goal %zu: (%.1f, %.1f)", goal_index_, x, y);
    nav_client_->async_send_goal(goal, opts);
  }

  rclcpp::Publisher<geometry_msgs::msg::PoseWithCovarianceStamped>::SharedPtr initialpose_pub_;
  rclcpp_action::Client<NavigateToPose>::SharedPtr nav_client_;
  rclcpp::TimerBase::SharedPtr initialpose_timer_;
  rclcpp::TimerBase::SharedPtr start_timer_;
  rclcpp::TimerBase::SharedPtr next_goal_timer_;

  std::vector<std::pair<double, double>> waypoints_;
  size_t goal_index_;
  bool initialpose_sent_ = false;
  bool started_ = false;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<AutoNavigator>());
  rclcpp::shutdown();
  return 0;
}
