// approach_client.cpp — 對 smart_brake_v2 的 Approach action 送 goal
//
// 用法：
//   ros2 run phase08_pkg approach_client 0.6 0.3
//   參數1: target_distance (公尺)
//   參數2: approach_speed   (m/s)
//
// 流程：
//   1. 建 ActionClient
//   2. 等 server 上線
//   3. 送 goal + 註冊 feedback callback
//   4. 等 goal handle 確認被接受
//   5. 等 result，印出最終結果

#include "rclcpp/rclcpp.hpp"
#include "rclcpp_action/rclcpp_action.hpp"
#include "my_robot_interfaces/action/approach.hpp"

using Approach = my_robot_interfaces::action::Approach;
using GoalHandle = rclcpp_action::ClientGoalHandle<Approach>;

class ApproachClient : public rclcpp::Node
{
public:
    ApproachClient(double target, double speed)
        : Node("approach_client"), target_(target), speed_(speed)
    {
        client_ = rclcpp_action::create_client<Approach>(this, "approach");
    }

    void send()
    {
        if (!client_->wait_for_action_server(std::chrono::seconds(5))) {
            RCLCPP_ERROR(get_logger(), "Action server not available");
            return;
        }

        auto goal = Approach::Goal();
        goal.target_distance = static_cast<float>(target_);
        goal.approach_speed = static_cast<float>(speed_);

        auto opts = rclcpp_action::Client<Approach>::SendGoalOptions();
        // Feedback callback: server 持續送進度
        opts.feedback_callback =
            [this](GoalHandle::SharedPtr,
                   const std::shared_ptr<const Approach::Feedback> fb) {
                RCLCPP_INFO(get_logger(),
                            "[Feedback] dist=%.2fm, %.1fs elapsed: %s",
                            fb->current_distance,
                            fb->elapsed_seconds,
                            fb->status.c_str());
            };
        // Goal 是否被接受
        opts.goal_response_callback =
            [this](GoalHandle::SharedPtr handle) {
                if (!handle) {
                    RCLCPP_ERROR(get_logger(), "Goal rejected by server");
                } else {
                    RCLCPP_INFO(get_logger(), "Goal accepted, waiting for result...");
                }
            };
        // Result callback
        opts.result_callback =
            [this](const GoalHandle::WrappedResult & wr) {
                switch (wr.code) {
                    case rclcpp_action::ResultCode::SUCCEEDED:
                        RCLCPP_INFO(get_logger(),
                            "✅ SUCCESS: final_dist=%.2fm in %.1fs",
                            wr.result->final_distance,
                            wr.result->elapsed_seconds);
                        break;
                    case rclcpp_action::ResultCode::ABORTED:
                        RCLCPP_ERROR(get_logger(), "❌ ABORTED");
                        break;
                    case rclcpp_action::ResultCode::CANCELED:
                        RCLCPP_WARN(get_logger(), "⚠️ CANCELED");
                        break;
                    default:
                        RCLCPP_ERROR(get_logger(), "Unknown result code");
                }
                rclcpp::shutdown();
            };

        client_->async_send_goal(goal, opts);
    }

private:
    double target_;
    double speed_;
    rclcpp_action::Client<Approach>::SharedPtr client_;
};

int main(int argc, char ** argv)
{
    rclcpp::init(argc, argv);

    double target = (argc > 1) ? std::stod(argv[1]) : 0.6;
    double speed = (argc > 2) ? std::stod(argv[2]) : 0.3;

    auto node = std::make_shared<ApproachClient>(target, speed);
    node->send();
    rclcpp::spin(node);
    return 0;
}
