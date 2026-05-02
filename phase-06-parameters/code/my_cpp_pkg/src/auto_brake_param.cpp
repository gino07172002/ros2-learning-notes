#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/twist.hpp"
#include "sensor_msgs/msg/point_cloud2.hpp"
#include "sensor_msgs/point_cloud2_iterator.hpp"
// rcl_interfaces：ROS 2 為「節點治理 API」（param、log、service introspection）
// 提供的訊息與服務型別。SetParametersResult 用於回報參數變更是否被接受。
#include "rcl_interfaces/msg/set_parameters_result.hpp"

using std::placeholders::_1;

// 在 Phase 03 的 AutoBrakeNode 基礎上加入 ROS 2 Parameter 系統。
// 三個原本寫死的常數變成可從外部調整的 param。
class AutoBrakeParamNode : public rclcpp::Node
{
public:
    AutoBrakeParamNode() : Node("auto_brake_param_node")
    {
        // === 步驟 1：宣告參數 ===
        // declare_parameter 做兩件事：
        //   1. 給 param 一個預設值（如果 YAML / CLI 沒覆蓋就用這個）
        //   2. 把它註冊到 ROS graph，讓 ros2 param list / rqt_reconfigure 看得到
        // 沒 declare 過的 param 名稱讀取時會丟例外（嚴格型別）。
        this->declare_parameter<double>("safe_distance", 1.0);
        this->declare_parameter<double>("max_speed", 0.2);
        this->declare_parameter<double>("corridor_width", 0.4);

        // === 步驟 2：第一次讀取，把值快取到成員變數 ===
        // 為什麼要快取？因為 callback 是高頻觸發（光達 10Hz），
        // 每次都呼叫 get_parameter 會多一層查表開銷。快取 + on_set_callback 才是慣用法。
        safe_distance_ = this->get_parameter("safe_distance").as_double();
        max_speed_ = this->get_parameter("max_speed").as_double();
        corridor_width_ = this->get_parameter("corridor_width").as_double();

        // === 步驟 3：訂閱參數變更事件 ===
        // add_on_set_parameters_callback 註冊一個「攔截器」，當有人呼叫
        // ros2 param set 時，這個 callback 在「值真的被改之前」執行——
        // 你可以驗證新值（例如 safe_distance 不能 < 0），不接受就回 successful=false。
        // 接受了之後 ROS 才會更新內部值。
        param_callback_handle_ = this->add_on_set_parameters_callback(
            std::bind(&AutoBrakeParamNode::on_param_change, this, _1));

        publisher_ = this->create_publisher<geometry_msgs::msg::Twist>("cmd_vel", 10);
        subscriber_ = this->create_subscription<sensor_msgs::msg::PointCloud2>(
            "lidar_points", rclcpp::SensorDataQoS(),
            std::bind(&AutoBrakeParamNode::cloud_callback, this, _1));

        RCLCPP_INFO(this->get_logger(),
                    "Started with: safe_distance=%.2f, max_speed=%.2f, corridor_width=%.2f",
                    safe_distance_, max_speed_, corridor_width_);
    }

private:
    // 參數成員變數 + 控制邏輯需要的訂閱/發布
    double safe_distance_;
    double max_speed_;
    double corridor_width_;

    rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr publisher_;
    rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::SharedPtr subscriber_;
    // ⚠️ callback handle 必須保留——析構就等於取消訂閱。沒存的話 callback 不會被呼叫。
    OnSetParametersCallbackHandle::SharedPtr param_callback_handle_;

    // 參數變更攔截器：驗證 + 套用新值
    rcl_interfaces::msg::SetParametersResult on_param_change(
        const std::vector<rclcpp::Parameter> & params)
    {
        rcl_interfaces::msg::SetParametersResult result;
        result.successful = true;

        for (const auto & p : params) {
            if (p.get_name() == "safe_distance") {
                double v = p.as_double();
                // 驗證：負距離不合理。回 successful=false 並寫 reason，
                // 呼叫端（ros2 param set）會看到失敗訊息。
                if (v < 0.0) {
                    result.successful = false;
                    result.reason = "safe_distance must be >= 0";
                    return result;
                }
                safe_distance_ = v;
                RCLCPP_INFO(this->get_logger(), "safe_distance -> %.2f", v);
            } else if (p.get_name() == "max_speed") {
                double v = p.as_double();
                if (v < 0.0 || v > 2.0) {
                    result.successful = false;
                    result.reason = "max_speed must be in [0, 2.0]";
                    return result;
                }
                max_speed_ = v;
                RCLCPP_INFO(this->get_logger(), "max_speed -> %.2f", v);
            } else if (p.get_name() == "corridor_width") {
                corridor_width_ = p.as_double();
                RCLCPP_INFO(this->get_logger(), "corridor_width -> %.2f", p.as_double());
            }
        }
        return result;
    }

    void cloud_callback(const sensor_msgs::msg::PointCloud2::SharedPtr msg)
    {
        auto twist_msg = geometry_msgs::msg::Twist();
        float min_forward_distance = 100.0f;

        sensor_msgs::PointCloud2ConstIterator<float> iter_x(*msg, "x");
        sensor_msgs::PointCloud2ConstIterator<float> iter_y(*msg, "y");

        // corridor_width / 2 給左右各一半
        const float half_corridor = static_cast<float>(corridor_width_) / 2.0f;

        for (; iter_x != iter_x.end(); ++iter_x, ++iter_y) {
            float x = *iter_x;
            float y = *iter_y;
            if (x > 0.0f && std::abs(y) < half_corridor) {
                if (x < min_forward_distance) {
                    min_forward_distance = x;
                }
            }
        }

        // 用快取的 param 值做判斷——比每次 get_parameter 快
        if (min_forward_distance > static_cast<float>(safe_distance_)) {
            twist_msg.linear.x = max_speed_;
            twist_msg.angular.z = 0.0;
            RCLCPP_INFO_THROTTLE(
                this->get_logger(), *this->get_clock(), 1000,
                "Clear (closest=%.2fm, threshold=%.2fm). Speed=%.2f",
                min_forward_distance, safe_distance_, max_speed_);
        } else {
            twist_msg.linear.x = 0.0;
            twist_msg.angular.z = 0.0;
            RCLCPP_WARN_THROTTLE(
                this->get_logger(), *this->get_clock(), 1000,
                "BRAKING (obstacle=%.2fm < threshold=%.2fm)",
                min_forward_distance, safe_distance_);
        }

        publisher_->publish(twist_msg);
    }
};

int main(int argc, char * argv[])
{
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<AutoBrakeParamNode>());
    rclcpp::shutdown();
    return 0;
}
