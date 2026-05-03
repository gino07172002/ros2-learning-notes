// test_brake_calculator.cpp
// gtest 單元測試 — 純邏輯測試，不需要 rclcpp runtime
//
// 結構：
//   TEST(TestSuiteName, TestCaseName) { ... }
// 內含 EXPECT_EQ / EXPECT_NEAR / EXPECT_TRUE 等斷言

#include <gtest/gtest.h>
#include "../src/brake_calculator.hpp"

using phase12::BrakeCalculator;

// === Test Suite: BrakeCalculatorTest ===

// 1. 沒障礙物 → 全速
TEST(BrakeCalculatorTest, NoObstacleReturnsFullSpeed) {
    BrakeCalculator calc(1.0, 0.3);
    EXPECT_DOUBLE_EQ(calc.compute(5.0, 0.5), 0.5);
    EXPECT_DOUBLE_EQ(calc.compute(100.0, 1.0), 1.0);
}

// 2. 障礙物在安全距離內 → 減速
TEST(BrakeCalculatorTest, NearObstacleSlowsDown) {
    BrakeCalculator calc(1.0, 0.3);
    EXPECT_NEAR(calc.compute(0.5, 0.5), 0.15, 1e-9);  // 0.5 * 0.3
    EXPECT_NEAR(calc.compute(0.99, 1.0), 0.30, 1e-9);
}

// 3. 邊界：剛好等於 safe_distance
TEST(BrakeCalculatorTest, ExactlyAtSafeDistanceSlowsDown) {
    BrakeCalculator calc(1.0, 0.3);
    EXPECT_NEAR(calc.compute(1.0, 0.5), 0.15, 1e-9);  // = safe_distance 也算近
}

// 4. 負距離（壞資料）→ 完全停
TEST(BrakeCalculatorTest, NegativeDistanceStops) {
    BrakeCalculator calc(1.0, 0.3);
    EXPECT_DOUBLE_EQ(calc.compute(-1.0, 0.5), 0.0);
}

// 5. is_slowing 邏輯
TEST(BrakeCalculatorTest, IsSlowingWhenInRange) {
    BrakeCalculator calc(1.0, 0.3);
    EXPECT_FALSE(calc.is_slowing(2.0));   // 太遠
    EXPECT_TRUE(calc.is_slowing(0.5));    // 範圍內
    EXPECT_TRUE(calc.is_slowing(1.0));    // 邊界
    EXPECT_FALSE(calc.is_slowing(-0.5));  // 負值
}

// 6. 不同 slowdown_factor 設定
TEST(BrakeCalculatorTest, DifferentSlowdownFactors) {
    BrakeCalculator calc_strict(1.0, 0.0);   // 完全停
    BrakeCalculator calc_lenient(1.0, 0.7);  // 70% 速度
    EXPECT_DOUBLE_EQ(calc_strict.compute(0.5, 1.0), 0.0);
    EXPECT_DOUBLE_EQ(calc_lenient.compute(0.5, 1.0), 0.7);
}

int main(int argc, char ** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
