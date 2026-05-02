#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/twist.hpp"
// PointCloud2：3D 點雲訊息格式。資料欄位以 binary blob 存放，需配合 iterator 解析。
#include "sensor_msgs/msg/point_cloud2.hpp"
// PointCloud2Iterator：讀取 PointCloud2 內 x/y/z/rgb 等欄位的安全 iterator。
#include "sensor_msgs/point_cloud2_iterator.hpp"

using std::placeholders::_1;  // std::bind 的第一個參數佔位符（給 callback 的 msg）

// 這個 Node 同時是 Publisher（發 cmd_vel）+ Subscriber（聽光達）——
// 在 ROS 2 中一個 Node 可以同時擔任多個角色，不像 MQTT 通常一個 client 只做一件事。
class AutoBrakeNode : public rclcpp::Node
{
public:
    AutoBrakeNode() : Node("auto_brake_node")
    {
        publisher_ = this->create_publisher<geometry_msgs::msg::Twist>("cmd_vel", 10);

        // 建立 Subscriber：類比 MQTT client.subscribe()，差別是：
        // 1) 訊息型別在編譯期決定（PointCloud2），收到的 msg 已被反序列化成 struct
        // 2) Callback 與 publisher 共用同一個事件迴圈（spin），所以 callback 不要阻塞太久
        subscriber_ = this->create_subscription<sensor_msgs::msg::PointCloud2>(
            "lidar_points",
            // ⚠️ QoS 關鍵：感測器資料必須用 SensorDataQoS()。
            // ROS 2 的 QoS 比 MQTT 複雜：Reliability/Durability/History 三維度都要對齊
            // publisher 端，否則「無聲失敗」——訊息在發但收不到，且不會報錯。
            // SensorDataQoS = Best Effort + Volatile + KeepLast(5)，配合多數光達/相機驅動。
            rclcpp::SensorDataQoS(),
            std::bind(&AutoBrakeNode::cloud_callback, this, _1));

        // RCLCPP_INFO 是 ROS 2 的 logger 巨集（不要用 std::cout）：
        // log 會送到 /rosout topic，可被 rqt_console 集中查看、被 ros2 bag 錄起來。
        RCLCPP_INFO(this->get_logger(), "3D Auto Brake Started!");
    }

private:
    // Callback 的 msg 是 SharedPtr<const T>，因為訊息可能被多個訂閱者共用，
    // 寫入會破壞其他訂閱者看到的資料——所以拿到的是 const view。
    void cloud_callback(const sensor_msgs::msg::PointCloud2::SharedPtr msg)
    {
        auto twist_msg = geometry_msgs::msg::Twist();
        float min_forward_distance = 100.0f;  // 沒障礙物時的「無限遠」哨兵值

        // PointCloud2 的 raw data 是 byte stream，欄位 layout 由 msg->fields 描述。
        // PointCloud2ConstIterator 幫你按欄位名稱抓正確的 offset，不用手動算 bytes。
        // 注意：x 與 y 是同步前進的（同一個點的 x/y），不是兩個獨立陣列。
        sensor_msgs::PointCloud2ConstIterator<float> iter_x(*msg, "x");
        sensor_msgs::PointCloud2ConstIterator<float> iter_y(*msg, "y");

        for (; iter_x != iter_x.end(); ++iter_x, ++iter_y) {
            float x = *iter_x;
            float y = *iter_y;

            // 篩選正前方 40cm 寬走廊內的點：
            // - x > 0：點在機器人前方（光達座標系：x 軸朝前）
            // - |y| < 0.2：左右各 20cm 範圍（避免被牆壁誤判）
            if (x > 0.0f && std::abs(y) < 0.2f) {
                if (x < min_forward_distance) {
                    min_forward_distance = x;
                }
            }
        }

        // 簡單的閾值煞車邏輯：< 1.0 m 就停，否則維持 0.2 m/s 前進
        if (min_forward_distance > 1.0f) {
            twist_msg.linear.x = 0.2;
            twist_msg.angular.z = 0.0;
            // _THROTTLE 版：限制每 1000ms (1 秒) 最多印一次。
            // 光達回呼頻率約 10Hz，沒節流會讓 terminal 被洗版。
            RCLCPP_INFO_THROTTLE(
                this->get_logger(), *this->get_clock(), 1000,
                "Clear ahead (Closest: %.2fm). Moving forward...", min_forward_distance);
        } else {
            twist_msg.linear.x = 0.0;
            twist_msg.angular.z = 0.0;
            RCLCPP_WARN_THROTTLE(
                this->get_logger(), *this->get_clock(), 1000,
                "Obstacle detected at %.2fm! BRAKING!", min_forward_distance);
        }

        // 在 subscriber callback 裡 publish 是常見模式（reactive control）。
        // 但要小心：callback 不能阻塞太久，否則整個 spin 會卡。
        publisher_->publish(twist_msg);
    }

    rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr publisher_;
    rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::SharedPtr subscriber_;
};

int main(int argc, char * argv[])
{
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<AutoBrakeNode>());
    rclcpp::shutdown();
    return 0;
}
