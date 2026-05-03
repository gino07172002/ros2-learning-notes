// lifecycle_demo.cpp
// LifecycleNode 完整生命週期 demo
//
// 一般 Node 啟動 = 立刻運作；LifecycleNode 多了「狀態機」：
//   unconfigured → inactive → active → inactive → finalized
//   每個狀態切換可在 callback 內做事（讀設定檔、開硬體、釋放資源）
//
// 業界用途：
//   - 啟動順序保證（確保所有 Node 都 configured 才一起 activate）
//   - 紅樓夢式 graceful shutdown（先 deactivate 再 cleanup）
//   - 故障隔離（某個 Node 進 ErrorProcessing 不影響其他）
//   - Nav2 / MoveIt 全部用 LifecycleNode
//
// 用法：
//   Terminal 1: ros2 run phase09_pkg lifecycle_demo
//   Terminal 2: ros2 lifecycle list /lifecycle_demo_node
//              ros2 lifecycle set /lifecycle_demo_node configure
//              ros2 lifecycle set /lifecycle_demo_node activate
//              ros2 lifecycle set /lifecycle_demo_node deactivate
//              ros2 lifecycle set /lifecycle_demo_node cleanup

#include "rclcpp/rclcpp.hpp"
#include "rclcpp_lifecycle/lifecycle_node.hpp"
#include "rclcpp_lifecycle/lifecycle_publisher.hpp"
#include "std_msgs/msg/string.hpp"

using namespace std::chrono_literals;
using LifecycleCallbackReturn = rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn;

class LifecycleDemoNode : public rclcpp_lifecycle::LifecycleNode
{
public:
    LifecycleDemoNode() : LifecycleNode("lifecycle_demo_node")
    {
        RCLCPP_INFO(get_logger(), "Constructed (state: unconfigured)");
    }

    // === 狀態切換 callback ===
    // 1. configure: 一次性設定（讀檔、宣告 publisher）
    LifecycleCallbackReturn on_configure(const rclcpp_lifecycle::State &) override
    {
        RCLCPP_INFO(get_logger(), "→ on_configure: declare publisher");
        // 在 configure 階段建立 publisher（但還不能發送）
        publisher_ = create_publisher<std_msgs::msg::String>("heartbeat", 10);
        return LifecycleCallbackReturn::SUCCESS;
    }

    // 2. activate: 開始實際運作（啟動 timer / 開始發送）
    LifecycleCallbackReturn on_activate(const rclcpp_lifecycle::State &) override
    {
        RCLCPP_INFO(get_logger(), "→ on_activate: start timer + publisher");
        publisher_->on_activate();           // ⚠️ LifecyclePublisher 必須手動 activate
        timer_ = create_wall_timer(1s, [this]() {
            auto msg = std_msgs::msg::String();
            msg.data = "I am ALIVE at " + std::to_string(now().seconds());
            publisher_->publish(msg);
            RCLCPP_INFO(get_logger(), "[ACTIVE] publishing: %s", msg.data.c_str());
        });
        return LifecycleCallbackReturn::SUCCESS;
    }

    // 3. deactivate: 暫停（停 timer，但保留 publisher）
    LifecycleCallbackReturn on_deactivate(const rclcpp_lifecycle::State &) override
    {
        RCLCPP_INFO(get_logger(), "→ on_deactivate: stop timer");
        timer_.reset();                       // 釋放 timer = 停止觸發
        publisher_->on_deactivate();
        return LifecycleCallbackReturn::SUCCESS;
    }

    // 4. cleanup: 釋放所有資源（回到 unconfigured 狀態）
    LifecycleCallbackReturn on_cleanup(const rclcpp_lifecycle::State &) override
    {
        RCLCPP_INFO(get_logger(), "→ on_cleanup: release publisher");
        publisher_.reset();
        return LifecycleCallbackReturn::SUCCESS;
    }

    // 5. shutdown: 從任何狀態進 finalized（Node 即將被銷毀）
    LifecycleCallbackReturn on_shutdown(const rclcpp_lifecycle::State & state) override
    {
        RCLCPP_WARN(get_logger(), "→ on_shutdown from state: %s", state.label().c_str());
        return LifecycleCallbackReturn::SUCCESS;
    }

private:
    std::shared_ptr<rclcpp_lifecycle::LifecyclePublisher<std_msgs::msg::String>> publisher_;
    rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char ** argv)
{
    rclcpp::init(argc, argv);
    auto node = std::make_shared<LifecycleDemoNode>();
    rclcpp::spin(node->get_node_base_interface());
    rclcpp::shutdown();
    return 0;
}
