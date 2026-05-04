// test_healthy_lifecycle.cpp — Phase 37
//
// 5 個 case:
//   1. InitialStateUnconfigured — ctor 後狀態 = unconfigured
//   2. ConfigureMovesToInactive — call on_configure 後 state = inactive
//   3. ActivateStartsTicking — activate + simulate_tick → tick_count > 0
//   4. ErrorInjectStopsTickAndSetsLastError — inject_error 後 simulate 不增加 tick
//   5. CleanupResetsTickCount — cleanup 後 tick_count 歸零

#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <memory>

#include "rclcpp/rclcpp.hpp"
#include "my_lifecycle_diag/healthy_lifecycle_node.hpp"

using namespace std::chrono_literals;
using my_lifecycle_diag::HealthyLifecycleNode;
using rclcpp_lifecycle::State;

class LifecycleFixture : public ::testing::Test
{
protected:
  void SetUp() override
  {
    if (!rclcpp::ok()) {
      rclcpp::init(0, nullptr);
    }
    node_ = std::make_shared<HealthyLifecycleNode>();
  }

  void TearDown() override
  {
    node_.reset();
  }

  // 直接呼 lifecycle interface 的 trigger 函式,不走 service
  void configure() { node_->configure(); }
  void activate()  { node_->activate(); }
  void deactivate(){ node_->deactivate(); }
  void cleanup()   { node_->cleanup(); }

  std::shared_ptr<HealthyLifecycleNode> node_;
};

TEST_F(LifecycleFixture, InitialStateUnconfigured)
{
  EXPECT_EQ(node_->current_state_label(), "unconfigured");
  EXPECT_EQ(node_->tick_count(), 0u);
}

TEST_F(LifecycleFixture, ConfigureMovesToInactive)
{
  configure();
  EXPECT_EQ(node_->current_state_label(), "inactive");
}

TEST_F(LifecycleFixture, ActivateStartsTicking)
{
  configure();
  activate();
  EXPECT_EQ(node_->current_state_label(), "active");
  // 直接呼 simulate_tick 不靠 timer(避免 WSL timer jitter)
  node_->simulate_tick();
  node_->simulate_tick();
  node_->simulate_tick();
  EXPECT_EQ(node_->tick_count(), 3u);
}

TEST_F(LifecycleFixture, ErrorInjectStopsTickAndSetsLastError)
{
  configure();
  activate();
  node_->simulate_tick();           // tick=1
  node_->inject_error("sensor_lost");
  node_->simulate_tick();           // 不算
  EXPECT_EQ(node_->tick_count(), 1u);
  // 注入後再來 tick 應該又能正常累計
  node_->simulate_tick();           // tick=2
  EXPECT_EQ(node_->tick_count(), 2u);
}

TEST_F(LifecycleFixture, CleanupResetsTickCount)
{
  configure();
  activate();
  node_->simulate_tick();
  node_->simulate_tick();
  EXPECT_EQ(node_->tick_count(), 2u);
  deactivate();
  cleanup();
  EXPECT_EQ(node_->tick_count(), 0u);
  EXPECT_EQ(node_->current_state_label(), "unconfigured");
}

int main(int argc, char ** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  int rc = RUN_ALL_TESTS();
  if (rclcpp::ok()) rclcpp::shutdown();
  return rc;
}
