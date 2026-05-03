// test_speed_policy.cpp — Capstone 1 純邏輯單元測試
//
// 證明把邏輯抽出來的好處：不用 init rclcpp 也能測

#include <gtest/gtest.h>
#include "../src/approach_controller.hpp"

using capstone1::SpeedPolicy;

TEST(SpeedPolicyTest, FullSpeedWhenClear) {
    SpeedPolicy policy(1.0, 0.3);
    EXPECT_DOUBLE_EQ(policy.compute_speed(5.0, 0.5), 0.5);
}

TEST(SpeedPolicyTest, SlowsDownNearObstacle) {
    SpeedPolicy policy(1.0, 0.3);
    EXPECT_NEAR(policy.compute_speed(0.5, 0.5), 0.15, 1e-9);
}

TEST(SpeedPolicyTest, StopsOnNegativeDistance) {
    SpeedPolicy policy(1.0, 0.3);
    EXPECT_DOUBLE_EQ(policy.compute_speed(-0.5, 0.5), 0.0);
}

TEST(SpeedPolicyTest, ReachedTargetWithinRange) {
    SpeedPolicy policy(1.0, 0.3);
    EXPECT_TRUE(policy.reached_target(0.5, 0.6));    // 0.5m <= target 0.6m
    EXPECT_FALSE(policy.reached_target(0.7, 0.6));   // 0.7m > target 0.6m
    EXPECT_FALSE(policy.reached_target(-1.0, 0.6));  // 負值不算到達
}

TEST(SpeedPolicyTest, ReachedTargetExactBoundary) {
    SpeedPolicy policy(1.0, 0.3);
    EXPECT_TRUE(policy.reached_target(0.6, 0.6));    // 邊界算到達
}

int main(int argc, char ** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
