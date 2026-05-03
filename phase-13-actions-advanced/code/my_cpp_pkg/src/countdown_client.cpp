// countdown_client.cpp
// 對 countdown_server 送 goal，可以中途用 SIGINT 取消
//
// 用法：
//   ros2 run phase13_pkg countdown_client 5      # 倒數 5
//   ros2 run phase13_pkg countdown_client 13     # 會 abort（內部模擬）
//   ros2 run phase13_pkg countdown_client 100    # 會 reject（太大）
//   啟動後按 Ctrl+C 中途取消（送 cancel）

#include "rclcpp/rclcpp.hpp"
#include "rclcpp_action/rclcpp_action.hpp"
#include "my_robot_interfaces/action/countdown.hpp"
#include <csignal>

using Countdown = my_robot_interfaces::action::Countdown;
using GoalHandle = rclcpp_action::ClientGoalHandle<Countdown>;

class CountdownClient : public rclcpp::Node
{
public:
    CountdownClient(int start) : Node("countdown_client"), start_(start)
    {
        client_ = rclcpp_action::create_client<Countdown>(this, "countdown");
    }

    void send_goal()
    {
        if (!client_->wait_for_action_server(std::chrono::seconds(5))) {
            RCLCPP_ERROR(get_logger(), "Server unavailable");
            rclcpp::shutdown();
            return;
        }

        auto goal = Countdown::Goal();
        goal.start_number = start_;

        auto opts = rclcpp_action::Client<Countdown>::SendGoalOptions();
        opts.goal_response_callback =
            [this](GoalHandle::SharedPtr handle) {
                if (!handle) {
                    RCLCPP_ERROR(get_logger(), "❌ Goal REJECTED by server");
                    rclcpp::shutdown();
                } else {
                    RCLCPP_INFO(get_logger(), "✅ Goal accepted");
                    goal_handle_ = handle;
                }
            };

        opts.feedback_callback =
            [this](GoalHandle::SharedPtr,
                   const std::shared_ptr<const Countdown::Feedback> fb) {
                RCLCPP_INFO(get_logger(),
                    "[Feedback] current=%d", fb->current_number);
            };

        opts.result_callback =
            [this](const GoalHandle::WrappedResult & wr) {
                switch (wr.code) {
                    case rclcpp_action::ResultCode::SUCCEEDED:
                        RCLCPP_INFO(get_logger(),
                            "🎉 SUCCEEDED: reached %d", wr.result->reached);
                        break;
                    case rclcpp_action::ResultCode::ABORTED:
                        RCLCPP_ERROR(get_logger(),
                            "💥 ABORTED at %d", wr.result->reached);
                        break;
                    case rclcpp_action::ResultCode::CANCELED:
                        RCLCPP_WARN(get_logger(),
                            "⚠️ CANCELED at %d", wr.result->reached);
                        break;
                    default:
                        RCLCPP_ERROR(get_logger(), "Unknown result code");
                }
                rclcpp::shutdown();
            };

        client_->async_send_goal(goal, opts);
    }

    // 收到 SIGINT 時呼叫——送 cancel request
    void cancel_goal()
    {
        if (goal_handle_) {
            RCLCPP_WARN(get_logger(), "Sending cancel request");
            client_->async_cancel_goal(goal_handle_);
        }
    }

private:
    int start_;
    rclcpp_action::Client<Countdown>::SharedPtr client_;
    GoalHandle::SharedPtr goal_handle_;
};

// 全域 client pointer 給 signal handler 用
std::shared_ptr<CountdownClient> g_client;

void sigint_handler(int)
{
    if (g_client) g_client->cancel_goal();
}

int main(int argc, char ** argv)
{
    int start = (argc > 1) ? std::stoi(argv[1]) : 5;

    rclcpp::init(argc, argv);
    g_client = std::make_shared<CountdownClient>(start);
    g_client->send_goal();

    // 安裝 SIGINT handler — 第一次 Ctrl+C 送 cancel，第二次才真的關
    std::signal(SIGINT, sigint_handler);

    rclcpp::spin(g_client);
    rclcpp::shutdown();
    return 0;
}
