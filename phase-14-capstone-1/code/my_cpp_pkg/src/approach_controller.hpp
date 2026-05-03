// approach_controller.hpp
// 純邏輯計算（給單元測試用）
// 把控制邏輯抽出來，不依賴 rclcpp，方便獨立測試

#pragma once

namespace capstone1 {

class SpeedPolicy {
public:
    SpeedPolicy(double safe_distance, double slowdown_factor)
    : safe_distance_(safe_distance), slowdown_factor_(slowdown_factor) {}

    // 根據障礙物距離決定速度
    double compute_speed(double obstacle_distance, double max_speed) const {
        if (obstacle_distance < 0.0) return 0.0;
        if (obstacle_distance > safe_distance_) return max_speed;
        return max_speed * slowdown_factor_;
    }

    // 是否已達 approach 目標
    bool reached_target(double obstacle_distance, double target) const {
        return obstacle_distance >= 0.0 && obstacle_distance <= target;
    }

private:
    double safe_distance_;
    double slowdown_factor_;
};

}  // namespace capstone1
