// healthy_lifecycle_node.hpp — Phase 37
//
// LifecycleNode + diagnostic_updater 的整合模板:
//   - 每個 transition (configure/activate/deactivate/cleanup) 自動發 diagnostic
//   - 提供一個假工作 (timer 跑 callback) 作為「正在做事」的 metric
//   - tick_count / last_error 全部反映在 /diagnostics
//   - 給 gtest fixture 用的同步 trigger API
//
// 業界實機 ROS 2 node 的標準骨架 — 上線時客戶期待 lifecycle + diagnostics
// 都已經接好。

#ifndef MY_LIFECYCLE_DIAG__HEALTHY_LIFECYCLE_NODE_HPP_
#define MY_LIFECYCLE_DIAG__HEALTHY_LIFECYCLE_NODE_HPP_

#include <atomic>
#include <memory>
#include <string>

#include "rclcpp/rclcpp.hpp"
#include "rclcpp_lifecycle/lifecycle_node.hpp"
#include "diagnostic_updater/diagnostic_updater.hpp"

namespace my_lifecycle_diag
{

using CallbackReturn = rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn;

class HealthyLifecycleNode : public rclcpp_lifecycle::LifecycleNode
{
public:
  explicit HealthyLifecycleNode(const rclcpp::NodeOptions & options = rclcpp::NodeOptions());

  // 5 個 lifecycle transitions
  CallbackReturn on_configure(const rclcpp_lifecycle::State &) override;
  CallbackReturn on_activate(const rclcpp_lifecycle::State &) override;
  CallbackReturn on_deactivate(const rclcpp_lifecycle::State &) override;
  CallbackReturn on_cleanup(const rclcpp_lifecycle::State &) override;
  CallbackReturn on_shutdown(const rclcpp_lifecycle::State &) override;

  // gtest 用:當前狀態 (label like "active") 跟 tick 累計
  // ⚠️ 雷:LifecycleNode::get_current_state() 上游不是 const method,
  // 所以這個 wrapper 也不能標 const(編譯期錯誤 -fpermissive)
  std::string current_state_label();
  uint64_t tick_count() const { return tick_count_.load(); }

  // gtest 用:模擬一次工作 tick(不走 timer)
  void simulate_tick();

  // gtest 用:強制下次 simulate_tick / timer 觸發 error 路徑
  void inject_error(const std::string & reason);

private:
  void on_work_timer();
  void produce_diagnostic(diagnostic_updater::DiagnosticStatusWrapper & stat);

  std::atomic<uint64_t> tick_count_{0};
  std::string last_error_;       // 由 inject_error 設,error 觸發後 work tick 寫進來
  std::string injected_error_;   // 待觸發的錯誤(下次 tick 用)
  bool active_{false};

  rclcpp::TimerBase::SharedPtr work_timer_;
  std::shared_ptr<diagnostic_updater::Updater> updater_;
};

}  // namespace my_lifecycle_diag

#endif  // MY_LIFECYCLE_DIAG__HEALTHY_LIFECYCLE_NODE_HPP_
