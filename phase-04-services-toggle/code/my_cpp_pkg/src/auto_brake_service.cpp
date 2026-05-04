#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/twist.hpp"
#include "sensor_msgs/msg/point_cloud2.hpp"
#include "sensor_msgs/point_cloud2_iterator.hpp"
// std_srvs::SetBool：ROS 2 內建的標準服務型別。
// Request 含一個 bool data，Response 含 bool success + string message。
// 適合「開/關」這種一問一答的簡單動作。
#include "std_srvs/srv/set_bool.hpp"
#include <atomic>
#include <cmath>

using std::placeholders::_1;
using std::placeholders::_2;  // service callback 需要 (request, response) 兩個參數

// 比 Phase 03 多了 Service Server。Service ≠ Topic：
// - Topic: pub/sub 廣播（持續），類比 MQTT
// - Service: client/server 一問一答（單次），類比 HTTP RPC 或 gRPC unary call
class AutoBrakeServiceNode : public rclcpp::Node
{
public:
    AutoBrakeServiceNode() : Node("auto_brake_service_node")
    {
        publisher_ = this->create_publisher<geometry_msgs::msg::Twist>("cmd_vel", 10);

        subscriber_ = this->create_subscription<sensor_msgs::msg::PointCloud2>(
            "lidar_points", rclcpp::SensorDataQoS(),
            std::bind(&AutoBrakeServiceNode::cloud_callback, this, _1));

        // 建立 Service Server：當有 client 呼叫 /toggle_brake 時觸發 callback。
        // Service callback 簽章是 (Request const&, Response&)——回傳值寫進 response 物件。
        // 不像 Subscriber 是「訊息進來就跑」，Service 是「有請求才跑，且呼叫端會等回應」。
        service_ = this->create_service<std_srvs::srv::SetBool>(
            "toggle_brake",
            std::bind(&AutoBrakeServiceNode::toggle_brake_callback, this, _1, _2));

        RCLCPP_INFO(this->get_logger(),
                    "AEB Service ready: 'toggle_brake'");
    }

private:
    // ⚠️ atomic<bool>：在預設的 SingleThreadedExecutor 下其實不會 race，
    // 但養成習慣很重要——一旦切到 MultiThreadedExecutor（Phase 08），
    // service callback 與 sub callback 會在不同 thread，沒 atomic 就 UB。
    std::atomic<bool> is_brake_active_{true};

    void toggle_brake_callback(
        const std::shared_ptr<std_srvs::srv::SetBool::Request> request,
        std::shared_ptr<std_srvs::srv::SetBool::Response> response)
    {
        // 直接修改成員變數——其他 callback（cloud_callback）會看到新值。
        // 這就是 service 的典型用途：改變節點的「執行模式」。
        is_brake_active_ = request->data;
        response->success = true;  // 服務有確實執行（即使結果是「關閉」也算 success）
        response->message = is_brake_active_
            ? "Brake system ENABLED."
            : "Brake system DISABLED. Watch out!";
        // 用 WARN 等級讓這個事件在 rqt_console 醒目顯示，方便 debug
        RCLCPP_WARN(this->get_logger(), ">>> Service: %s <<<",
                    is_brake_active_ ? "ENABLED" : "DISABLED");
    }

    void cloud_callback(const sensor_msgs::msg::PointCloud2::SharedPtr msg)
    {
        // 開關關閉時直接 return——光達訊息照樣進來但不做任何反應。
        // 這個寫法比「停 subscription」乾淨（不用反覆 create/destroy）。
        if (!is_brake_active_) {
            RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 2000,
                "Brake system offline. Waiting for enable command...");
            return;
        }

        // 以下避障邏輯與 Phase 03 完全相同
        auto twist_msg = geometry_msgs::msg::Twist();
        float min_forward_distance = 100.0f;
        sensor_msgs::PointCloud2ConstIterator<float> iter_x(*msg, "x");
        sensor_msgs::PointCloud2ConstIterator<float> iter_y(*msg, "y");

        for (; iter_x != iter_x.end(); ++iter_x, ++iter_y) {
            float x = *iter_x;
            float y = *iter_y;
            if (x > 0.0f && std::abs(y) < 0.2f) {
                if (x < min_forward_distance) {
                    min_forward_distance = x;
                }
            }
        }

        if (min_forward_distance > 1.0f) {
            twist_msg.linear.x = 0.2;
            twist_msg.angular.z = 0.0;
        } else {
            twist_msg.linear.x = 0.0;
            twist_msg.angular.z = 0.0;
            RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 1000,
                "Obstacle detected at %.2fm! BRAKING!", min_forward_distance);
        }
        publisher_->publish(twist_msg);
    }

    rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr publisher_;
    rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::SharedPtr subscriber_;
    // Service 也是 SharedPtr 持有
    rclcpp::Service<std_srvs::srv::SetBool>::SharedPtr service_;
};

int main(int argc, char * argv[])
{
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<AutoBrakeServiceNode>());
    rclcpp::shutdown();
    return 0;
}
