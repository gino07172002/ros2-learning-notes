// countdown_server.cpp
// Phase 13 — Action Server 進階主題
//
// 比 Phase 08 的 Approach action 多了：
//   - 完整的 cancel 處理（含資源清理）
//   - abort（伺服器主動失敗）
//   - reject（拒絕 goal）
//   - feedback 中途的工作狀態變化
//
// 故事：從 N 倒數到 0
//   - start_number > 30：reject（太久不接）
//   - start_number == 13：執行到 7 自動 abort（模擬內部錯誤）
//   - 其他：正常倒數
//   - client 中途取消：cancel handler 觸發

#include "rclcpp/rclcpp.hpp"
#include "rclcpp_action/rclcpp_action.hpp"
#include "my_robot_interfaces/action/countdown.hpp"

using std::placeholders::_1;
using std::placeholders::_2;
using namespace std::chrono_literals;

using Countdown = my_robot_interfaces::action::Countdown;
using GoalHandle = rclcpp_action::ServerGoalHandle<Countdown>;

class CountdownServer : public rclcpp::Node
{
public:
    CountdownServer() : Node("countdown_server")
    {
        action_server_ = rclcpp_action::create_server<Countdown>(
            this,
            "countdown",
            std::bind(&CountdownServer::handle_goal, this, _1, _2),
            std::bind(&CountdownServer::handle_cancel, this, _1),
            std::bind(&CountdownServer::handle_accepted, this, _1));

        RCLCPP_INFO(get_logger(), "Countdown server ready (try numbers: 1-30)");
    }

private:
    rclcpp_action::Server<Countdown>::SharedPtr action_server_;

    // === handle_goal: 收到 goal 時決定接受/拒絕 ===
    rclcpp_action::GoalResponse handle_goal(
        const rclcpp_action::GoalUUID & uuid,
        std::shared_ptr<const Countdown::Goal> goal)
    {
        RCLCPP_INFO(get_logger(),
            "Received goal: count down from %d", goal->start_number);

        // Reject 條件：太大或負數
        if (goal->start_number > 30) {
            RCLCPP_WARN(get_logger(),
                "Rejecting goal: %d is too large (max 30)", goal->start_number);
            return rclcpp_action::GoalResponse::REJECT;
        }
        if (goal->start_number < 0) {
            RCLCPP_WARN(get_logger(),
                "Rejecting goal: %d is negative", goal->start_number);
            return rclcpp_action::GoalResponse::REJECT;
        }

        (void)uuid;
        return rclcpp_action::GoalResponse::ACCEPT_AND_EXECUTE;
    }

    // === handle_cancel: client 想取消時 ===
    // 不一定要接受——可以 REJECT 表示「我已經太晚停不下來」
    rclcpp_action::CancelResponse handle_cancel(
        const std::shared_ptr<GoalHandle> goal_handle)
    {
        RCLCPP_WARN(get_logger(), "Cancel request received");
        (void)goal_handle;
        return rclcpp_action::CancelResponse::ACCEPT;
    }

    // === handle_accepted: 開新 thread 執行任務 ===
    // 不能在 callback 內阻塞——會卡住整個 spin
    void handle_accepted(const std::shared_ptr<GoalHandle> goal_handle)
    {
        std::thread{std::bind(&CountdownServer::execute, this, _1), goal_handle}
            .detach();
    }

    // === 真正執行倒數的 thread ===
    void execute(const std::shared_ptr<GoalHandle> goal_handle)
    {
        const auto goal = goal_handle->get_goal();
        auto feedback = std::make_shared<Countdown::Feedback>();
        auto result = std::make_shared<Countdown::Result>();

        RCLCPP_INFO(get_logger(),
            "Executing countdown from %d", goal->start_number);

        rclcpp::Rate rate(1);  // 1Hz 倒數

        for (int i = goal->start_number; i >= 0 && rclcpp::ok(); --i) {
            // 1. 檢查 cancel
            if (goal_handle->is_canceling()) {
                result->success = false;
                result->reached = i;
                goal_handle->canceled(result);
                RCLCPP_WARN(get_logger(),
                    "Goal canceled at %d", i);
                return;
            }

            // 2. 檢查 abort 條件（goal=13 時跑到 7 自動失敗，模擬內部錯誤）
            if (goal->start_number == 13 && i == 7) {
                result->success = false;
                result->reached = i;
                goal_handle->abort(result);
                RCLCPP_ERROR(get_logger(),
                    "ABORTED at %d (simulated internal error)", i);
                return;
            }

            // 3. 發送 feedback
            feedback->current_number = i;
            goal_handle->publish_feedback(feedback);
            RCLCPP_INFO(get_logger(), "  ... %d", i);

            rate.sleep();
        }

        // 4. 正常完成
        if (rclcpp::ok()) {
            result->success = true;
            result->reached = 0;
            goal_handle->succeed(result);
            RCLCPP_INFO(get_logger(), "Countdown complete!");
        }
    }
};

int main(int argc, char ** argv)
{
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<CountdownServer>());
    rclcpp::shutdown();
    return 0;
}
