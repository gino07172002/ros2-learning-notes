// executors_demo.cpp
// 比較 SingleThreadedExecutor vs MultiThreadedExecutor
//
// 一個 Node 內有兩個 timer，一個跑慢 (3 秒 sleep)、一個跑快 (100ms 觸發)。
// 用兩種 Executor 跑，觀察結果差異：
//
// SingleThreadedExecutor:
//   慢 callback 執行時，快 callback 全部被卡住等待 → 「block」
//
// MultiThreadedExecutor:
//   兩個 callback 在不同 thread 並行 → 慢 callback 不會卡快 callback
//
// 用法：
//   ros2 run phase09_pkg executors_demo single   # 用 single-thread executor
//   ros2 run phase09_pkg executors_demo multi    # 用 multi-thread executor

#include "rclcpp/rclcpp.hpp"
#include <chrono>
#include <thread>
#include <string>

using namespace std::chrono_literals;

class TimerDemoNode : public rclcpp::Node
{
public:
    TimerDemoNode() : Node("timer_demo_node")
    {
        // ⚠️ 預設 callback group 是 MutuallyExclusive（即使用 MultiThreadedExecutor 也會序列化）
        // 用 Reentrant group 才能真正並行
        reentrant_group_ = create_callback_group(
            rclcpp::CallbackGroupType::Reentrant);

        // 慢 timer：每 5 秒觸發，每次 sleep 3 秒（模擬重計算）
        slow_timer_ = create_wall_timer(
            5s,
            [this]() {
                RCLCPP_WARN(get_logger(), "[SLOW] start (will sleep 3s)");
                std::this_thread::sleep_for(3s);
                RCLCPP_WARN(get_logger(), "[SLOW] done");
            },
            reentrant_group_);

        // 快 timer：每 100ms 觸發，幾乎瞬間完成
        fast_timer_ = create_wall_timer(
            100ms,
            [this]() {
                tick_count_++;
                if (tick_count_ % 10 == 0) {
                    RCLCPP_INFO(get_logger(), "[FAST] tick=%d", tick_count_);
                }
            },
            reentrant_group_);

        RCLCPP_INFO(get_logger(), "TimerDemoNode ready (callback_group=Reentrant)");
    }

private:
    rclcpp::CallbackGroup::SharedPtr reentrant_group_;
    rclcpp::TimerBase::SharedPtr slow_timer_;
    rclcpp::TimerBase::SharedPtr fast_timer_;
    int tick_count_ = 0;
};

int main(int argc, char ** argv)
{
    rclcpp::init(argc, argv);

    auto node = std::make_shared<TimerDemoNode>();

    // 用 CLI 參數選 executor
    std::string mode = (argc > 1) ? argv[1] : "single";

    if (mode == "multi") {
        RCLCPP_INFO(node->get_logger(), "Using MultiThreadedExecutor");
        // 預設 thread 數 = std::thread::hardware_concurrency()
        rclcpp::executors::MultiThreadedExecutor executor;
        executor.add_node(node);
        executor.spin();
    } else {
        RCLCPP_INFO(node->get_logger(), "Using SingleThreadedExecutor");
        // rclcpp::spin() 內部就是 SingleThreadedExecutor
        rclcpp::spin(node);
    }

    rclcpp::shutdown();
    return 0;
}
