// aggressive_strategy.cpp
// 激進策略：障礙物近時減速 30% 但繼續走（不停）

#include "brake_strategy_base/brake_strategy.hpp"
#include <pluginlib/class_list_macros.hpp>

namespace brake_strategy_plugins
{

class AggressiveStrategy : public brake_strategy_base::BrakeStrategy
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
        return max_speed_ * 0.3;  // 激進：減速但不停
    }

    const char * name() const override { return "Aggressive"; }

private:
    double safe_distance_ = 1.0;
    double max_speed_ = 0.5;
};

}  // namespace brake_strategy_plugins

PLUGINLIB_EXPORT_CLASS(
    brake_strategy_plugins::AggressiveStrategy,
    brake_strategy_base::BrakeStrategy)
