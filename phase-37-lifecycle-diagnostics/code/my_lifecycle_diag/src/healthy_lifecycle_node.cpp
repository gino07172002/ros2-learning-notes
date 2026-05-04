// healthy_lifecycle_node.cpp — Phase 37

#include "my_lifecycle_diag/healthy_lifecycle_node.hpp"

#include <chrono>
#include <functional>

#include "diagnostic_msgs/msg/diagnostic_status.hpp"

namespace my_lifecycle_diag
{

using namespace std::chrono_literals;
using std::placeholders::_1;
using diagnostic_msgs::msg::DiagnosticStatus;

HealthyLifecycleNode::HealthyLifecycleNode(const rclcpp::NodeOptions & options)
: rclcpp_lifecycle::LifecycleNode("healthy_lifecycle_node", options)
{
  // ⚠️ 雷:LifecycleNode 不能直接傳 this 給 Updater(會炸 bad_weak_ptr,
  // 因為 LifecycleNode 內部 NodeBaseInterface 跟普通 Node 取法不同)。
  // 解法:把 Updater 建立挪到 on_configure() — 那時 shared_from_this 已生效。
  RCLCPP_INFO(get_logger(), "ctor: state=%s", current_state_label().c_str());
}

CallbackReturn HealthyLifecycleNode::on_configure(const rclcpp_lifecycle::State &)
{
  RCLCPP_INFO(get_logger(), "on_configure");
  // 在 configure 期建 Updater(不再有 bad_weak_ptr 問題)
  updater_ = std::make_shared<diagnostic_updater::Updater>(this);
  updater_->setHardwareID("healthy_lifecycle_node");
  updater_->add("LifecycleHealth",
                std::bind(&HealthyLifecycleNode::produce_diagnostic, this, _1));
  return CallbackReturn::SUCCESS;
}

CallbackReturn HealthyLifecycleNode::on_activate(const rclcpp_lifecycle::State &)
{
  RCLCPP_INFO(get_logger(), "on_activate");
  active_ = true;
  // 100ms 跑一次工作 — 累計 tick_count
  work_timer_ = create_wall_timer(
    100ms, std::bind(&HealthyLifecycleNode::on_work_timer, this));
  return CallbackReturn::SUCCESS;
}

CallbackReturn HealthyLifecycleNode::on_deactivate(const rclcpp_lifecycle::State &)
{
  RCLCPP_INFO(get_logger(), "on_deactivate");
  active_ = false;
  work_timer_.reset();
  return CallbackReturn::SUCCESS;
}

CallbackReturn HealthyLifecycleNode::on_cleanup(const rclcpp_lifecycle::State &)
{
  RCLCPP_INFO(get_logger(), "on_cleanup");
  updater_.reset();
  tick_count_ = 0;
  last_error_.clear();
  return CallbackReturn::SUCCESS;
}

CallbackReturn HealthyLifecycleNode::on_shutdown(const rclcpp_lifecycle::State &)
{
  RCLCPP_INFO(get_logger(), "on_shutdown");
  active_ = false;
  work_timer_.reset();
  updater_.reset();
  return CallbackReturn::SUCCESS;
}

void HealthyLifecycleNode::on_work_timer()
{
  if (!active_) return;
  simulate_tick();
}

void HealthyLifecycleNode::simulate_tick()
{
  if (!injected_error_.empty()) {
    last_error_ = injected_error_;
    injected_error_.clear();
    return;  // tick 不算
  }
  tick_count_.fetch_add(1);
}

void HealthyLifecycleNode::inject_error(const std::string & reason)
{
  injected_error_ = reason;
}

std::string HealthyLifecycleNode::current_state_label() const
{
  return get_current_state().label();
}

void HealthyLifecycleNode::produce_diagnostic(
  diagnostic_updater::DiagnosticStatusWrapper & stat)
{
  const auto label = current_state_label();
  stat.add("lifecycle_state", label);
  stat.add("tick_count", std::to_string(tick_count_.load()));
  stat.add("active", active_ ? "yes" : "no");
  stat.add("last_error", last_error_.empty() ? "(none)" : last_error_);

  // 規則:
  //   - inactive (configured but not activated):WARN「閒置中」
  //   - active + 有 last_error:ERROR
  //   - active 且乾淨:OK
  //   - 其他(unconfigured / finalized):WARN
  if (!last_error_.empty() && active_) {
    stat.summary(DiagnosticStatus::ERROR, "Active but had error: " + last_error_);
  } else if (active_) {
    stat.summary(DiagnosticStatus::OK,
                 "Active, " + std::to_string(tick_count_.load()) + " ticks");
  } else if (label == "inactive") {
    stat.summary(DiagnosticStatus::WARN, "Configured but not activated");
  } else {
    stat.summary(DiagnosticStatus::WARN, "Lifecycle state: " + label);
  }
}

}  // namespace my_lifecycle_diag
