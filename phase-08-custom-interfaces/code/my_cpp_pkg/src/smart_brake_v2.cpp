// smart_brake_v2.cpp — Phase 07 升級版，全用 Custom Interfaces
//
// 跟 Phase 07 比的差異：
//   - Topic 廣播改用自訂 BrakeStatus.msg（一個訊息塞 mode + speed + distance + uptime + text）
//   - Service 改用自訂 SetBrakeMode.srv（一次設模式+速度上限+log 訊息）
//   - 新增：Approach.action server（接 client 任務「靠近到 X 公尺」）
//
// 角色：
//   - Subscriber: PointCloud2 from /lidar_points
//   - Publisher:  Twist to /cmd_vel
//   - Publisher:  BrakeStatus to /brake_status (1Hz, 自訂型別!)
//   - Service Server: /set_brake_mode (自訂型別!)
//   - Action Server:  /approach (自訂型別!)

#include "rclcpp/rclcpp.hpp"
#include "rclcpp_action/rclcpp_action.hpp"
#include "geometry_msgs/msg/twist.hpp"
#include "sensor_msgs/msg/point_cloud2.hpp"
#include "sensor_msgs/point_cloud2_iterator.hpp"

// ⚠️ 引入自訂 interface 的標頭檔。
// 路徑慣例：<package_name>/<msg|srv|action>/<snake_case_filename>.hpp
// 例如：BrakeStatus.msg → my_robot_interfaces/msg/brake_status.hpp
//      SetBrakeMode.srv → my_robot_interfaces/srv/set_brake_mode.hpp
// rosidl 自動把 PascalCase 的 msg 名轉成 snake_case 檔名。
#include "my_robot_interfaces/msg/brake_status.hpp"
#include "my_robot_interfaces/srv/set_brake_mode.hpp"
#include "my_robot_interfaces/action/approach.hpp"

#include <atomic>
#include <chrono>

using std::placeholders::_1;
using std::placeholders::_2;
using namespace std::chrono_literals;

// 縮短型別名
using BrakeStatus = my_robot_interfaces::msg::BrakeStatus;
using SetBrakeMode = my_robot_interfaces::srv::SetBrakeMode;
using Approach = my_robot_interfaces::action::Approach;
using GoalHandleApproach = rclcpp_action::ServerGoalHandle<Approach>;

class SmartBrakeV2 : public rclcpp::Node
{
public:
    SmartBrakeV2() : Node("smart_brake_v2")
    {
        start_time_ = this->now();

        // === Publisher: cmd_vel + brake_status ===
        cmd_pub_ = this->create_publisher<geometry_msgs::msg::Twist>("cmd_vel", 10);
        // 自訂訊息的 Publisher 跟標準訊息語法一樣，差別只在 template 參數
        status_pub_ = this->create_publisher<BrakeStatus>("brake_status", 10);

        // === Subscriber: lidar ===
        lidar_sub_ = this->create_subscription<sensor_msgs::msg::PointCloud2>(
            "lidar_points", rclcpp::SensorDataQoS(),
            std::bind(&SmartBrakeV2::cloud_callback, this, _1));

        // === Service Server: SetBrakeMode (自訂) ===
        mode_service_ = this->create_service<SetBrakeMode>(
            "set_brake_mode",
            std::bind(&SmartBrakeV2::set_mode_callback, this, _1, _2));

        // === Action Server: Approach (自訂) ===
        // rclcpp_action::create_server 簽章較複雜，要傳三個 callback：
        //   1. handle_goal: 收到 client 的 goal 時決定接受/拒絕
        //   2. handle_cancel: client 想取消時決定要不要讓他取消
        //   3. handle_accepted: goal 被接受後在哪個 thread 執行任務
        approach_server_ = rclcpp_action::create_server<Approach>(
            this,
            "approach",
            std::bind(&SmartBrakeV2::handle_goal, this, _1, _2),
            std::bind(&SmartBrakeV2::handle_cancel, this, _1),
            std::bind(&SmartBrakeV2::handle_accepted, this, _1));

        // === Timer: cmd_vel 100ms + brake_status 1Hz ===
        cmd_timer_ = this->create_wall_timer(
            100ms, std::bind(&SmartBrakeV2::publish_cmd, this));
        status_timer_ = this->create_wall_timer(
            1s, std::bind(&SmartBrakeV2::publish_status, this));

        RCLCPP_INFO(get_logger(), "smart_brake_v2 ready (mode=ENABLED)");
    }

private:
    // === 共享狀態 (atomic) ===
    std::atomic<uint8_t> mode_{BrakeStatus::MODE_ENABLED};   // 用 IDL 生成的常數！
    std::atomic<double> max_speed_{0.5};
    std::atomic<double> last_min_distance_{100.0};
    std::atomic<double> current_speed_{0.0};                 // 當前實際送出的速度
    rclcpp::Time start_time_;

    rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr cmd_pub_;
    rclcpp::Publisher<BrakeStatus>::SharedPtr status_pub_;
    rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::SharedPtr lidar_sub_;
    rclcpp::Service<SetBrakeMode>::SharedPtr mode_service_;
    rclcpp_action::Server<Approach>::SharedPtr approach_server_;
    rclcpp::TimerBase::SharedPtr cmd_timer_;
    rclcpp::TimerBase::SharedPtr status_timer_;

    // ============================================================
    // === Lidar callback: 算最近障礙物距離 ===
    // ============================================================
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

    // ============================================================
    // === Service callback: SetBrakeMode 自訂型別 ===
    // ============================================================
    void set_mode_callback(
        const std::shared_ptr<SetBrakeMode::Request> req,
        std::shared_ptr<SetBrakeMode::Response> res)
    {
        // 驗證 mode
        if (req->mode > BrakeStatus::MODE_EMERGENCY) {
            res->success = false;
            res->message = "Invalid mode (must be 0/1/2)";
            return;
        }

        uint8_t prev = mode_.load();
        mode_ = req->mode;
        res->previous_mode = prev;

        // max_speed < 0 表示「不改變」
        if (req->max_speed >= 0.0f) {
            max_speed_ = req->max_speed;
        }
        res->applied_max_speed = static_cast<float>(max_speed_.load());

        res->success = true;
        res->message = "OK: " + req->reason;

        RCLCPP_WARN(get_logger(), ">>> mode %u -> %u (reason: %s)",
                    prev, req->mode, req->reason.c_str());
    }

    // ============================================================
    // === Action callbacks ===
    // ============================================================
    rclcpp_action::GoalResponse handle_goal(
        const rclcpp_action::GoalUUID & /*uuid*/,
        std::shared_ptr<const Approach::Goal> goal)
    {
        RCLCPP_INFO(get_logger(),
                    "Received approach goal: target=%.2fm speed=%.2f",
                    goal->target_distance, goal->approach_speed);
        // 驗證
        if (goal->target_distance < 0.0f || goal->approach_speed <= 0.0f) {
            RCLCPP_WARN(get_logger(), "Rejecting goal (invalid params)");
            return rclcpp_action::GoalResponse::REJECT;
        }
        return rclcpp_action::GoalResponse::ACCEPT_AND_EXECUTE;
    }

    rclcpp_action::CancelResponse handle_cancel(
        const std::shared_ptr<GoalHandleApproach> /*goal_handle*/)
    {
        RCLCPP_WARN(get_logger(), "Cancel request received");
        return rclcpp_action::CancelResponse::ACCEPT;
    }

    void handle_accepted(const std::shared_ptr<GoalHandleApproach> goal_handle)
    {
        // 不在 callback 內阻塞——開新 thread 執行任務
        std::thread{std::bind(&SmartBrakeV2::execute_approach, this, _1),
                    goal_handle}.detach();
    }

    void execute_approach(const std::shared_ptr<GoalHandleApproach> goal_handle)
    {
        const auto goal = goal_handle->get_goal();
        auto feedback = std::make_shared<Approach::Feedback>();
        auto result = std::make_shared<Approach::Result>();
        rclcpp::Time start = this->now();

        // 暫存原始模式，任務結束復原
        uint8_t saved_mode = mode_.load();
        double saved_max = max_speed_.load();
        mode_ = BrakeStatus::MODE_ENABLED;
        max_speed_ = goal->approach_speed;

        rclcpp::Rate loop_rate(5);  // 5Hz 送 feedback
        while (rclcpp::ok()) {
            // Cancel 處理
            if (goal_handle->is_canceling()) {
                result->success = false;
                result->final_distance = static_cast<float>(last_min_distance_.load());
                result->elapsed_seconds = (this->now() - start).seconds();
                goal_handle->canceled(result);
                mode_ = saved_mode;
                max_speed_ = saved_max;
                RCLCPP_WARN(get_logger(), "Approach canceled");
                return;
            }

            double dist = last_min_distance_.load();
            double elapsed = (this->now() - start).seconds();

            // Feedback 發送
            feedback->current_distance = static_cast<float>(dist);
            feedback->elapsed_seconds = static_cast<float>(elapsed);
            if (dist > goal->target_distance) {
                feedback->status = "Approaching...";
            } else {
                feedback->status = "Reached target";
            }
            goal_handle->publish_feedback(feedback);

            // 達標則 succeed
            if (dist <= goal->target_distance) {
                result->success = true;
                result->final_distance = static_cast<float>(dist);
                result->elapsed_seconds = static_cast<float>(elapsed);
                goal_handle->succeed(result);
                mode_ = saved_mode;
                max_speed_ = saved_max;
                RCLCPP_INFO(get_logger(),
                            "Approach succeeded at %.2fm (took %.1fs)",
                            dist, elapsed);
                return;
            }

            // 超時保護 (30s)
            if (elapsed > 30.0) {
                result->success = false;
                result->final_distance = static_cast<float>(dist);
                result->elapsed_seconds = static_cast<float>(elapsed);
                goal_handle->abort(result);
                mode_ = saved_mode;
                max_speed_ = saved_max;
                RCLCPP_ERROR(get_logger(), "Approach timeout");
                return;
            }

            loop_rate.sleep();
        }
    }

    // ============================================================
    // === Publish timers ===
    // ============================================================
    void publish_cmd()
    {
        auto twist = geometry_msgs::msg::Twist();
        const uint8_t mode = mode_.load();
        const double max_v = max_speed_.load();
        const double dist = last_min_distance_.load();

        if (mode == BrakeStatus::MODE_DISABLED) {
            twist.linear.x = max_v;
        } else if (mode == BrakeStatus::MODE_EMERGENCY) {
            twist.linear.x = 0.0;  // 緊急停車
        } else {  // ENABLED
            twist.linear.x = (dist > 1.0) ? max_v : (max_v * 0.3);
        }

        current_speed_ = twist.linear.x;
        cmd_pub_->publish(twist);
    }

    void publish_status()
    {
        BrakeStatus msg;
        msg.header.stamp = this->now();
        msg.header.frame_id = "smart_brake_v2";
        msg.mode = mode_.load();
        msg.current_speed = static_cast<float>(current_speed_.load());
        msg.closest_obstacle_distance = static_cast<float>(last_min_distance_.load());
        msg.uptime_seconds = (this->now() - start_time_).seconds();

        // 給人看的訊息
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
    rclcpp::spin(std::make_shared<SmartBrakeV2>());
    rclcpp::shutdown();
    return 0;
}
