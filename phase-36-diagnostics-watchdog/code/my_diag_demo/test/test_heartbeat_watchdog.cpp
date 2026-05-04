// test_heartbeat_watchdog.cpp — Phase 36
//
// 4 個 case:
//   1. NeverSeen — 沒喂過心跳,time_since_last_beat_ns 回 -1
//   2. AfterBeat — 喂一筆,age 接近 0
//   3. AgesIncreaseOverTime — 等一下,age 確實增加
//   4. MultiTopicIndependent — 兩個 topic 各自獨立紀錄

#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <memory>

#include "rclcpp/rclcpp.hpp"
#include "my_diag_demo/heartbeat_watchdog.hpp"

using namespace std::chrono_literals;
using my_diag_demo::HeartbeatWatchdog;

class WatchdogFixture : public ::testing::Test
{
protected:
  void SetUp() override
  {
    if (!rclcpp::ok()) {
      rclcpp::init(0, nullptr);
    }
    rclcpp::NodeOptions opts;
    opts.parameter_overrides({
      rclcpp::Parameter("watched_topics",
        std::vector<std::string>{"/lidar_hb", "/imu_hb"}),
      rclcpp::Parameter("timeout_sec", 1.0),
    });
    node_ = std::make_shared<HeartbeatWatchdog>(opts);
  }

  void TearDown() override
  {
    node_.reset();
  }

  std::shared_ptr<HeartbeatWatchdog> node_;
};

TEST_F(WatchdogFixture, NeverSeenReturnsMinusOne)
{
  EXPECT_EQ(node_->time_since_last_beat_ns("/lidar_hb"), -1);
  EXPECT_EQ(node_->time_since_last_beat_ns("/imu_hb"), -1);
}

TEST_F(WatchdogFixture, AfterBeatAgeIsSmall)
{
  node_->simulate_beat("/lidar_hb");
  // 不睡覺,馬上問,應該 < 100ms
  int64_t age_ns = node_->time_since_last_beat_ns("/lidar_hb");
  EXPECT_GE(age_ns, 0);
  EXPECT_LT(age_ns, 100'000'000);  // 100ms
}

TEST_F(WatchdogFixture, AgesIncreaseOverTime)
{
  node_->simulate_beat("/lidar_hb");
  std::this_thread::sleep_for(150ms);
  int64_t age_ns = node_->time_since_last_beat_ns("/lidar_hb");
  // 至少 100ms,但 sleep_for 不一定剛好 150ms,寬鬆 50ms
  EXPECT_GT(age_ns, 100'000'000);
  EXPECT_LT(age_ns, 500'000'000);
}

TEST_F(WatchdogFixture, MultiTopicIndependent)
{
  node_->simulate_beat("/lidar_hb");
  std::this_thread::sleep_for(50ms);
  node_->simulate_beat("/imu_hb");

  int64_t lidar_age = node_->time_since_last_beat_ns("/lidar_hb");
  int64_t imu_age = node_->time_since_last_beat_ns("/imu_hb");

  EXPECT_GT(lidar_age, imu_age);  // lidar 比較久之前打的
  EXPECT_GT(lidar_age - imu_age, 30'000'000);  // 差至少 30ms
}

int main(int argc, char ** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  int rc = RUN_ALL_TESTS();
  if (rclcpp::ok()) {
    rclcpp::shutdown();
  }
  return rc;
}
