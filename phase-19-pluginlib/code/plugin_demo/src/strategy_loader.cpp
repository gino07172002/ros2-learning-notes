// strategy_loader.cpp
// pluginlib 主程式：runtime 載入指定的 plugin
//
// 用法：
//   ros2 run plugin_demo strategy_loader Conservative
//   ros2 run plugin_demo strategy_loader Aggressive
//
// 同一支程式，傳不同 CLI 參數載不同行為——這就是 pluginlib 的價值

#include "brake_strategy_base/brake_strategy.hpp"
#include "rclcpp/rclcpp.hpp"
#include <pluginlib/class_loader.hpp>

#include <memory>
#include <string>

class StrategyDemo : public rclcpp::Node
{
public:
    StrategyDemo(const std::string & strategy_name) : Node("strategy_demo")
    {
        // ClassLoader: (套件名, 完整 base class 名稱)
        loader_ = std::make_shared<pluginlib::ClassLoader<brake_strategy_base::BrakeStrategy>>(
            "brake_strategy_base", "brake_strategy_base::BrakeStrategy");

        // 從 CLI 名稱建出對應的 plugin
        std::string fully_qualified =
            "brake_strategy_plugins::" + strategy_name + "Strategy";

        try {
            strategy_ = loader_->createSharedInstance(fully_qualified);
            strategy_->initialize(/*safe_distance=*/1.0, /*max_speed=*/0.5);
            RCLCPP_INFO(get_logger(),
                "Loaded strategy: %s", strategy_->name());
        } catch (const pluginlib::PluginlibException & e) {
            RCLCPP_ERROR(get_logger(), "Failed to load: %s", e.what());
            rclcpp::shutdown();
            return;
        }

        // 用幾個固定距離測試 plugin 行為
        for (double d : {2.0, 0.8, 0.3}) {
            double v = strategy_->compute_speed(d);
            RCLCPP_INFO(get_logger(),
                "  obstacle=%.1fm → speed=%.2fm/s", d, v);
        }
    }

private:
    std::shared_ptr<pluginlib::ClassLoader<brake_strategy_base::BrakeStrategy>> loader_;
    std::shared_ptr<brake_strategy_base::BrakeStrategy> strategy_;
};

int main(int argc, char ** argv)
{
    rclcpp::init(argc, argv);

    std::string strategy_name = (argc > 1) ? argv[1] : "Conservative";
    auto node = std::make_shared<StrategyDemo>(strategy_name);

    // 不需要 spin——demo 直接結束就好
    rclcpp::shutdown();
    return 0;
}
