// approach_controller.cpp
// Capstone 1：整合所有 Part 3 學的東西的綜合作品
//
// 整合：
//   - Phase 08 Custom Interfaces (Approach.action, BrakeStatus.msg, SetBrakeMode.srv)
//   - Phase 09 LifecycleNode（受控啟動/停止）
//   - Phase 11 Launch event_handler（呼叫 lifecycle 自動 activate）
//   - Phase 12 純邏輯抽離（SpeedPolicy 可測試）
//   - Phase 13 Action server（Approach 任務 + cancel/abort/succeed 全套）
//
// 角色：
//   - LifecycleNode（受控）
//   - Subscriber（lidar_points / PointCloud2）
//   - Publisher（cmd_vel / Twist）
//   - Publisher（brake_status / BrakeStatus）
//   - Service Server（set_brake_mode / SetBrakeMode）
//   - Action Server（approach / Approach）
//
// 一個 Node 同時擔任 6 個角色 — 這就是 Part 3 學完的「實力」

#include "rclcpp/rclcpp.hpp"
#include "rclcpp_lifecycle/lifecycle_node.hpp"
#include "rclcpp_action/rclcpp_action.hpp"
#include "geometry_msgs/msg/twist.hpp"
#include "sensor_msgs/msg/point_cloud2.hpp"
#include "sensor_msgs/point_cloud2_iterator.hpp"
#include "my_robot_interfaces/msg/brake_status.hpp"
#include "my_robot_interfaces/srv/set_brake_mode.hpp"
#include "my_robot_interfaces/action/approach.hpp"

#include "approach_controller.hpp"

#include <atomic>
#include <chrono>

using std::placeholders::_1;
using std::placeholders::_2;
using namespace std::chrono_literals;
using LifecycleCallbackReturn = rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn;

using BrakeStatus = my_robot_interfaces::msg::BrakeStatus;
using SetBrakeMode = my_robot_interfaces::srv::SetBrakeMode;
using Approach = my_robot_interfaces::action::Approach;
using ApproachGoalHandle = rclcpp_action::ServerGoalHandle<Approach>;

class ApproachController : public rclcpp_lifecycle::LifecycleNode
{
public:
    ApproachController()
    : LifecycleNode("approach_controller"),
      policy_(/*safe_distance=*/1.0, /*slowdown_factor=*/0.3)
    {
        RCLCPP_INFO(get_logger(), "[Capstone] Constructor done (state: unconfigured)");
    }

    // === Lifecycle: configure ===
    LifecycleCallbackReturn on_configure(const rclcpp_lifecycle::State &) override
    {
        RCLCPP_INFO(get_logger(), "[Capstone] on_configure");
        cmd_pub_ = create_publisher<geometry_msgs::msg::Twist>("cmd_vel", 10);
        status_pub_ = create_publisher<BrakeStatus>("brake_status", 10);
        return LifecycleCallbackReturn::SUCCESS;
    }

    // === Lifecycle: activate ===
    LifecycleCallbackReturn on_activate(const rclcpp_lifecycle::State &) override
    {
        RCLCPP_INFO(get_logger(), "[Capstone] on_activate");
        cmd_pub_->on_activate();
        status_pub_->on_activate();

        lidar_sub_ = create_subscription<sensor_msgs::msg::PointCloud2>(
            "lidar_points", rclcpp::SensorDataQoS(),
            std::bind(&ApproachController::cloud_callback, this, _1));

        mode_service_ = create_service<SetBrakeMode>(
            "set_brake_mode",
            std::bind(&ApproachController::set_mode_callback, this, _1, _2));

        approach_server_ = rclcpp_action::create_server<Approach>(
            this, "approach",
            std::bind(&ApproachController::handle_goal, this, _1, _2),
            std::bind(&ApproachController::handle_cancel, this, _1),
            std::bind(&ApproachController::handle_accepted, this, _1));

        cmd_timer_ = create_wall_timer(100ms,
            std::bind(&ApproachController::publish_cmd, this));
        status_timer_ = create_wall_timer(1s,
            std::bind(&ApproachController::publish_status, this));

        start_time_ = now();
        return LifecycleCallbackReturn::SUCCESS;
    }

    // === Lifecycle: deactivate ===
    LifecycleCallbackReturn on_deactivate(const rclcpp_lifecycle::State &) override
    {
        RCLCPP_INFO(get_logger(), "[Capstone] on_deactivate");
        cmd_timer_.reset();
        status_timer_.reset();
        lidar_sub_.reset();
        mode_service_.reset();
        approach_server_.reset();
        cmd_pub_->on_deactivate();
        status_pub_->on_deactivate();
        return LifecycleCallbackReturn::SUCCESS;
    }

    LifecycleCallbackReturn on_cleanup(const rclcpp_lifecycle::State &) override
    {
        RCLCPP_INFO(get_logger(), "[Capstone] on_cleanup");
        cmd_pub_.reset();
        status_pub_.reset();
        return LifecycleCallbackReturn::SUCCESS;
    }

private:
    capstone1::SpeedPolicy policy_;

    std::atomic<uint8_t> mode_{BrakeStatus::MODE_ENABLED};
    std::atomic<double> max_speed_{0.5};
    std::atomic<double> last_min_distance_{100.0};
    std::atomic<double> current_speed_{0.0};
    rclcpp::Time start_time_;

    std::shared_ptr<rclcpp_lifecycle::LifecyclePublisher<geometry_msgs::msg::Twist>> cmd_pub_;
    std::shared_ptr<rclcpp_lifecycle::LifecyclePublisher<BrakeStatus>> status_pub_;
    rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::SharedPtr lidar_sub_;
    rclcpp::Service<SetBrakeMode>::SharedPtr mode_service_;
    rclcpp_action::Server<Approach>::SharedPtr approach_server_;
    rclcpp::TimerBase::SharedPtr cmd_timer_;
    rclcpp::TimerBase::SharedPtr status_timer_;

    // === Subscriber callback ===
    void cloud_callback(const sensor_msgs::msg::PointCloud2::SharedPtr msg)
    {
        float min_dist = 100.0f;
        sensor_msgs::PointCloud2ConstIterator<float> ix(*msg, "x");
        sensor_msgs::PointCloud2ConstIterator<float> iy(*msg, "y");
        for (; ix != ix.end(); ++ix, ++iy) {
            if (*ix > 0.0f && std::abs(*iy) < 0.2f) {
                if (*ix < min_dist) min_dist = *ix;
            }
        }
        last_min_distance_ = min_dist;
    }

    // === Service callback ===
    void set_mode_callback(
        const std::shared_ptr<SetBrakeMode::Request> req,
        std::shared_ptr<SetBrakeMode::Response> res)
    {
        if (req->mode > BrakeStatus::MODE_EMERGENCY) {
            res->success = false;
            res->message = "Invalid mode";
            return;
        }
        uint8_t prev = mode_.load();
        mode_ = req->mode;
        if (req->max_speed >= 0.0f) max_speed_ = req->max_speed;
        res->success = true;
        res->previous_mode = prev;
        res->applied_max_speed = static_cast<float>(max_speed_.load());
        res->message = "OK: " + req->reason;
        RCLCPP_WARN(get_logger(), ">>> mode %u -> %u", prev, req->mode);
    }

    // === Action callbacks ===
    rclcpp_action::GoalResponse handle_goal(
        const rclcpp_action::GoalUUID &,
        std::shared_ptr<const Approach::Goal> goal)
    {
        RCLCPP_INFO(get_logger(),
            "Approach goal: target=%.2fm speed=%.2f",
            goal->target_distance, goal->approach_speed);
        if (goal->target_distance < 0.0f || goal->approach_speed <= 0.0f) {
            return rclcpp_action::GoalResponse::REJECT;
        }
        return rclcpp_action::GoalResponse::ACCEPT_AND_EXECUTE;
    }

    rclcpp_action::CancelResponse handle_cancel(const std::shared_ptr<ApproachGoalHandle>)
    {
        return rclcpp_action::CancelResponse::ACCEPT;
    }

    void handle_accepted(const std::shared_ptr<ApproachGoalHandle> goal_handle)
    {
        std::thread{std::bind(&ApproachController::execute_approach, this, _1),
                    goal_handle}.detach();
    }

    void execute_approach(const std::shared_ptr<ApproachGoalHandle> goal_handle)
    {
        const auto goal = goal_handle->get_goal();
        auto feedback = std::make_shared<Approach::Feedback>();
        auto result = std::make_shared<Approach::Result>();
        rclcpp::Time start = now();

        uint8_t saved_mode = mode_.load();
        double saved_max = max_speed_.load();
        mode_ = BrakeStatus::MODE_ENABLED;
        max_speed_ = goal->approach_speed;

        rclcpp::Rate rate(5);
        while (rclcpp::ok()) {
            double dist = last_min_distance_.load();
            double elapsed = (now() - start).seconds();

            if (goal_handle->is_canceling()) {
                result->success = false;
                result->final_distance = static_cast<float>(dist);
                result->elapsed_seconds = static_cast<float>(elapsed);
                goal_handle->canceled(result);
                mode_ = saved_mode; max_speed_ = saved_max;
                return;
            }

            feedback->current_distance = static_cast<float>(dist);
            feedback->elapsed_seconds = static_cast<float>(elapsed);
            feedback->status = (dist > goal->target_distance) ? "Approaching" : "Reached";
            goal_handle->publish_feedback(feedback);

            // 用純邏輯類別檢查
            if (policy_.reached_target(dist, goal->target_distance)) {
                result->success = true;
                result->final_distance = static_cast<float>(dist);
                result->elapsed_seconds = static_cast<float>(elapsed);
                goal_handle->succeed(result);
                mode_ = saved_mode; max_speed_ = saved_max;
                return;
            }

            if (elapsed > 30.0) {
                result->success = false;
                result->final_distance = static_cast<float>(dist);
                result->elapsed_seconds = static_cast<float>(elapsed);
                goal_handle->abort(result);
                mode_ = saved_mode; max_speed_ = saved_max;
                return;
            }

            rate.sleep();
        }
    }

    // === Publish timers ===
    void publish_cmd()
    {
        auto twist = geometry_msgs::msg::Twist();
        const uint8_t mode = mode_.load();
        const double max_v = max_speed_.load();
        const double dist = last_min_distance_.load();

        if (mode == BrakeStatus::MODE_DISABLED) {
            twist.linear.x = max_v;
        } else if (mode == BrakeStatus::MODE_EMERGENCY) {
            twist.linear.x = 0.0;
        } else {
            // 用 SpeedPolicy 計算（純邏輯，可單元測試）
            twist.linear.x = policy_.compute_speed(dist, max_v);
        }

        current_speed_ = twist.linear.x;
        cmd_pub_->publish(twist);
    }

    void publish_status()
    {
        BrakeStatus msg;
        msg.header.stamp = now();
        msg.header.frame_id = "approach_controller";
        msg.mode = mode_.load();
        msg.current_speed = static_cast<float>(current_speed_.load());
        msg.closest_obstacle_distance = static_cast<float>(last_min_distance_.load());
        msg.uptime_seconds = (now() - start_time_).seconds();
        const char * mode_str =
            (msg.mode == BrakeStatus::MODE_DISABLED)  ? "DISABLED" :
            (msg.mode == BrakeStatus::MODE_EMERGENCY) ? "EMERGENCY" :
                                                        "ENABLED";
        char buf[128];
        snprintf(buf, sizeof(buf), "[%s] speed=%.2f obstacle=%.2fm",
                 mode_str, msg.current_speed, msg.closest_obstacle_distance);
        msg.status_text = buf;
        status_pub_->publish(msg);
    }
};

int main(int argc, char ** argv)
{
    rclcpp::init(argc, argv);
    auto node = std::make_shared<ApproachController>();
    rclcpp::spin(node->get_node_base_interface());
    rclcpp::shutdown();
    return 0;
}
