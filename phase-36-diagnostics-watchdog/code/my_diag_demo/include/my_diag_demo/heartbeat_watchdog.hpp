// heartbeat_watchdog.hpp — Phase 36
//
// 一個 watchdog node:訂閱 N 個 topic,要求每個 topic 在 timeout 內必須來一筆,
// 否則該 topic 標 ERROR,整個 system 健康狀態降級。
//
// 用 diagnostic_updater 自動往 /diagnostics 發狀態,給 aggregator / Foxglove 看。

#ifndef MY_DIAG_DEMO__HEARTBEAT_WATCHDOG_HPP_
#define MY_DIAG_DEMO__HEARTBEAT_WATCHDOG_HPP_

#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "rclcpp/rclcpp.hpp"
#include "diagnostic_updater/diagnostic_updater.hpp"
#include "std_msgs/msg/empty.hpp"

namespace my_diag_demo
{

class HeartbeatWatchdog : public rclcpp::Node
{
public:
  explicit HeartbeatWatchdog(const rclcpp::NodeOptions & options = rclcpp::NodeOptions());

  // 暴露給 gtest:回傳某 topic 距離上次心跳的 ns,沒收過回 -1
  int64_t time_since_last_beat_ns(const std::string & topic) const;

  // 暴露給 gtest:模擬一筆心跳到來(直接呼叫不走 ROS)
  void simulate_beat(const std::string & topic);

private:
  void produce_diagnostic(diagnostic_updater::DiagnosticStatusWrapper & stat);

  std::vector<std::string> watched_topics_;
  double timeout_sec_;

  // 各 topic 最後一次收到 message 的時間
  mutable std::mutex last_seen_mu_;
  std::map<std::string, rclcpp::Time> last_seen_;

  // 一個 sub per topic,全用 std_msgs/Empty 當 heartbeat
  std::vector<rclcpp::Subscription<std_msgs::msg::Empty>::SharedPtr> subs_;

  std::shared_ptr<diagnostic_updater::Updater> updater_;
};

}  // namespace my_diag_demo

#endif  // MY_DIAG_DEMO__HEARTBEAT_WATCHDOG_HPP_
