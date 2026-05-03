// conservative_strategy.cpp
// 保守策略：障礙物近就直接停（0 速度）
//
// 用 PLUGINLIB_EXPORT_CLASS 巨集向 pluginlib 註冊本 class
// 註冊後外部主程式才能 runtime 載入

#include "brake_strategy_base/brake_strategy.hpp"
#include <pluginlib/class_list_macros.hpp>

namespace brake_strategy_plugins
{

class ConservativeStrategy : public brake_strategy_base::BrakeStrategy
{
public:
    void initialize(double safe_distance, double max_speed) override
    {
        safe_distance_ = safe_distance;
        max_speed_ = max_speed;
    }

    double compute_speed(double obstacle_distance) const override
    {
        if (obstacle_distance < 0.0) return 0.0;
        if (obstacle_distance > safe_distance_) return max_speed_;
        return 0.0;  // 保守：直接停
    }

    const char * name() const override { return "Conservative"; }

private:
    double safe_distance_ = 1.0;
    double max_speed_ = 0.5;
};

}  // namespace brake_strategy_plugins

// ⚠️ 必須註冊：(具體 class, 抽象 base class)
PLUGINLIB_EXPORT_CLASS(
    brake_strategy_plugins::ConservativeStrategy,
    brake_strategy_base::BrakeStrategy)
