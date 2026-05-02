// Mini Capstone 1：整合 Phase 03/04/06 的所有東西
//
// 同時擔任三個角色：
//   1. Subscriber：訂閱 PointCloud2 光達資料（Phase 03）
//   2. Publisher：發 cmd_vel 控制烏龜（Phase 01/03）
//   3. Service Server：toggle_brake 遠端開關（Phase 04）
//   4. Parameter Holder：max_speed / safe_distance / corridor_width 可動態調整（Phase 06）
//
// 跟 Phase 04 的差別：
//   - Phase 04 障礙物近就「直接停」，沒障礙物就「全速 0.2」
//   - 本章「障礙物近 → 減半速度（不是停）」「沒障礙物 → max_speed 速度」
//   這讓你拖滑桿改 max_speed 時烏龜真的會慢/快變化，demo 才有戲。

#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/twist.hpp"
#include "sensor_msgs/msg/point_cloud2.hpp"
#include "sensor_msgs/point_cloud2_iterator.hpp"
#include "std_srvs/srv/set_bool.hpp"
#include "rcl_interfaces/msg/set_parameters_result.hpp"
#include <atomic>

using std::placeholders::_1;
using std::placeholders::_2;

class SmartBrakeNode : public rclcpp::Node
{
public:
    SmartBrakeNode() : Node("smart_brake_node")
    {
        // === Parameters (Phase 06) ===
        this->declare_parameter<double>("max_speed", 0.5);
        this->declare_parameter<double>("safe_distance", 1.0);
        this->declare_parameter<double>("corridor_width", 0.4);
        max_speed_ = this->get_parameter("max_speed").as_double();
        safe_distance_ = this->get_parameter("safe_distance").as_double();
        corridor_width_ = this->get_parameter("corridor_width").as_double();
        param_cb_handle_ = this->add_on_set_parameters_callback(
            std::bind(&SmartBrakeNode::on_param_change, this, _1));

        // === Publisher (Phase 01) ===
        publisher_ = this->create_publisher<geometry_msgs::msg::Twist>("cmd_vel", 10);

        // === Subscriber + QoS (Phase 03) ===
        subscriber_ = this->create_subscription<sensor_msgs::msg::PointCloud2>(
            "lidar_points", rclcpp::SensorDataQoS(),
            std::bind(&SmartBrakeNode::cloud_callback, this, _1));

        // === Service Server (Phase 04) ===
        service_ = this->create_service<std_srvs::srv::SetBool>(
            "toggle_brake",
            std::bind(&SmartBrakeNode::toggle_brake_callback, this, _1, _2));

        // 沒收到光達資料時也要動——用 timer 定期送目前的指令
        // 這樣即使 fake_lidar 還沒 spawn，烏龜也會自己往前。
        publish_timer_ = this->create_wall_timer(
            std::chrono::milliseconds(100),
            std::bind(&SmartBrakeNode::publish_cmd, this));

        RCLCPP_INFO(this->get_logger(),
                    "SmartBrake started: max_speed=%.2f safe_distance=%.2f",
                    max_speed_.load(), safe_distance_.load());
    }

private:
    // === Param 共享狀態都用 atomic（多 callback 安全）===
    std::atomic<double> max_speed_;
    std::atomic<double> safe_distance_;
    std::atomic<double> corridor_width_;
    std::atomic<bool> brake_enabled_{true};

    // === 從光達 callback 算出來的「最近障礙物距離」, cmd publish timer 會用它 ===
    std::atomic<double> last_min_distance_{100.0};

    rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr publisher_;
    rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::SharedPtr subscriber_;
    rclcpp::Service<std_srvs::srv::SetBool>::SharedPtr service_;
    rclcpp::TimerBase::SharedPtr publish_timer_;
    OnSetParametersCallbackHandle::SharedPtr param_cb_handle_;

    // === Param 變更攔截器 ===
    rcl_interfaces::msg::SetParametersResult on_param_change(
        const std::vector<rclcpp::Parameter> & params)
    {
        rcl_interfaces::msg::SetParametersResult result;
        result.successful = true;

        for (const auto & p : params) {
            const auto & name = p.get_name();
            if (name == "max_speed") {
                if (p.as_double() < 0.0 || p.as_double() > 5.0) {
                    result.successful = false;
                    result.reason = "max_speed must be in [0, 5.0]";
                    return result;
                }
                max_speed_ = p.as_double();
                RCLCPP_INFO(this->get_logger(), "max_speed -> %.2f", p.as_double());
            } else if (name == "safe_distance") {
                if (p.as_double() < 0.0) {
                    result.successful = false;
                    result.reason = "safe_distance must be >= 0";
                    return result;
                }
                safe_distance_ = p.as_double();
                RCLCPP_INFO(this->get_logger(), "safe_distance -> %.2f", p.as_double());
            } else if (name == "corridor_width") {
                corridor_width_ = p.as_double();
                RCLCPP_INFO(this->get_logger(), "corridor_width -> %.2f", p.as_double());
            }
        }
        return result;
    }

    // === Service callback ===
    void toggle_brake_callback(
        const std::shared_ptr<std_srvs::srv::SetBool::Request> request,
        std::shared_ptr<std_srvs::srv::SetBool::Response> response)
    {
        brake_enabled_ = request->data;
        response->success = true;
        response->message = brake_enabled_
            ? "Brake ENABLED — will slow down near obstacles."
            : "Brake DISABLED — will ignore obstacles, full speed ahead!";
        RCLCPP_WARN(this->get_logger(), ">>> Service: brake %s <<<",
                    brake_enabled_ ? "ENABLED" : "DISABLED");
    }

    // === Subscriber callback：算最近障礙物距離 ===
    void cloud_callback(const sensor_msgs::msg::PointCloud2::SharedPtr msg)
    {
        float min_dist = 100.0f;
        const float half_corridor = static_cast<float>(corridor_width_) / 2.0f;

        sensor_msgs::PointCloud2ConstIterator<float> iter_x(*msg, "x");
        sensor_msgs::PointCloud2ConstIterator<float> iter_y(*msg, "y");

        for (; iter_x != iter_x.end(); ++iter_x, ++iter_y) {
            float x = *iter_x;
            float y = *iter_y;
            if (x > 0.0f && std::abs(y) < half_corridor) {
                if (x < min_dist) min_dist = x;
            }
        }
        last_min_distance_ = min_dist;
    }

    // === Publish timer：根據最新狀態決定速度 ===
    // 跟 Phase 04 不同：障礙物近 = 速度減半（不是停），讓烏龜還是會動但變慢
    void publish_cmd()
    {
        auto twist = geometry_msgs::msg::Twist();
        const double max_speed = max_speed_.load();   // 一次 load，下面都用這個

        if (!brake_enabled_) {
            // 系統關閉：全速前進，無視障礙物
            twist.linear.x = max_speed;
            RCLCPP_INFO_THROTTLE(
                this->get_logger(), *this->get_clock(), 2000,
                "Brake offline. FULL SPEED %.2f m/s (ignoring obstacles)",
                max_speed);
        } else {
            const double dist = last_min_distance_.load();
            const double safe = safe_distance_.load();
            if (dist > safe) {
                // 安全：全速
                twist.linear.x = max_speed;
                RCLCPP_INFO_THROTTLE(
                    this->get_logger(), *this->get_clock(), 2000,
                    "Clear (closest=%.2fm). Speed %.2f m/s",
                    dist, max_speed);
            } else {
                // 有障礙：減半速（不是停）
                twist.linear.x = max_speed * 0.3;
                RCLCPP_WARN_THROTTLE(
                    this->get_logger(), *this->get_clock(), 1000,
                    "Obstacle %.2fm. Slowing to %.2f m/s",
                    dist, twist.linear.x);
            }
        }

        publisher_->publish(twist);
    }
};

int main(int argc, char * argv[])
{
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<SmartBrakeNode>());
    rclcpp::shutdown();
    return 0;
}
