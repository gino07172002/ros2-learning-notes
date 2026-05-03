// brake_calculator.hpp
// 純邏輯：給最近障礙物距離 + 上限速度 + 安全距離，回傳實際應該送的速度
//
// 把純邏輯抽出來做成 header-only library，方便單元測試（不需要 rclcpp）

#pragma once

namespace phase12 {

class BrakeCalculator {
public:
    BrakeCalculator(double safe_distance, double slowdown_factor)
    : safe_distance_(safe_distance), slowdown_factor_(slowdown_factor) {}

    // 主邏輯：根據距離計算速度
    double compute(double obstacle_distance, double max_speed) const {
        if (obstacle_distance < 0.0) {
            return 0.0;  // 邊界：負距離視為「碰到了」全停
        }
        if (obstacle_distance > safe_distance_) {
            return max_speed;
        }
        return max_speed * slowdown_factor_;
    }

    // 是否處於減速狀態
    bool is_slowing(double obstacle_distance) const {
        return obstacle_distance >= 0.0 && obstacle_distance <= safe_distance_;
    }

private:
    double safe_distance_;
    double slowdown_factor_;
};

}  // namespace phase12
