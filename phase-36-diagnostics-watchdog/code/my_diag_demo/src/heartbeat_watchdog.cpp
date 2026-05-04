// heartbeat_watchdog.cpp — Phase 36

#include "my_diag_demo/heartbeat_watchdog.hpp"

#include <chrono>
#include <functional>

namespace my_diag_demo
{

using std::placeholders::_1;
using namespace std::chrono_literals;

HeartbeatWatchdog::HeartbeatWatchdog(const rclcpp::NodeOptions & options)
: rclcpp::Node("heartbeat_watchdog", options)
{
  // 參數
  this->declare_parameter<std::vector<std::string>>(
    "watched_topics", std::vector<std::string>{"/lidar_hb", "/imu_hb"});
  this->declare_parameter<double>("timeout_sec", 1.0);

  watched_topics_ = this->get_parameter("watched_topics").as_string_array();
  timeout_sec_ = this->get_parameter("timeout_sec").as_double();

  RCLCPP_INFO(get_logger(), "Watching %zu topic(s), timeout=%.2fs",
              watched_topics_.size(), timeout_sec_);

  // 為每個 topic 建一個 sub
  for (const auto & topic : watched_topics_) {
    auto cb = [this, topic](std_msgs::msg::Empty::SharedPtr) {
      simulate_beat(topic);
    };
    auto sub = this->create_subscription<std_msgs::msg::Empty>(
      topic, rclcpp::QoS(10), cb);
    subs_.push_back(sub);
  }

  // diagnostic_updater 自動每秒(預設)往 /diagnostics 發
  // ⚠️ 必須在 Node 完整建構之後再 new — 不然 shared_from_this() 會炸
  updater_ = std::make_shared<diagnostic_updater::Updater>(this);
  updater_->setHardwareID("watchdog_demo");
  updater_->add("Heartbeats",
                std::bind(&HeartbeatWatchdog::produce_diagnostic, this, _1));
}

void HeartbeatWatchdog::simulate_beat(const std::string & topic)
{
  std::lock_guard<std::mutex> lk(last_seen_mu_);
  last_seen_[topic] = this->now();
}

int64_t HeartbeatWatchdog::time_since_last_beat_ns(const std::string & topic) const
{
  std::lock_guard<std::mutex> lk(last_seen_mu_);
  auto it = last_seen_.find(topic);
  if (it == last_seen_.end()) {
    return -1;
  }
  return (this->now() - it->second).nanoseconds();
}

void HeartbeatWatchdog::produce_diagnostic(
  diagnostic_updater::DiagnosticStatusWrapper & stat)
{
  using diagnostic_msgs::msg::DiagnosticStatus;

  size_t alive = 0, missing = 0, stale = 0;
  const auto now_t = this->now();
  const rclcpp::Duration timeout = rclcpp::Duration::from_seconds(timeout_sec_);

  std::lock_guard<std::mutex> lk(last_seen_mu_);
  for (const auto & topic : watched_topics_) {
    auto it = last_seen_.find(topic);
    if (it == last_seen_.end()) {
      stat.add(topic, "NEVER_SEEN");
      missing++;
      continue;
    }
    const auto age = now_t - it->second;
    if (age > timeout) {
      stat.add(topic, "STALE (" + std::to_string(age.seconds()) + "s ago)");
      stale++;
    } else {
      stat.add(topic, "ok (" + std::to_string(age.seconds()) + "s ago)");
      alive++;
    }
  }

  // 整體等級:有 missing → ERROR;有 stale → WARN;全 alive → OK
  if (missing > 0) {
    stat.summary(DiagnosticStatus::ERROR,
                 std::to_string(missing) + " topic(s) never seen");
  } else if (stale > 0) {
    stat.summary(DiagnosticStatus::WARN,
                 std::to_string(stale) + " topic(s) stale");
  } else {
    stat.summary(DiagnosticStatus::OK,
                 "All " + std::to_string(alive) + " heartbeats alive");
  }
}

}  // namespace my_diag_demo
