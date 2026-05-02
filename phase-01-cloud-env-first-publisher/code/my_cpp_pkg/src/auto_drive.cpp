// ROS 2 C++ 客戶端函式庫，提供 Node、Publisher、Timer 等核心 API
#include "rclcpp/rclcpp.hpp"
// Twist 訊息：機器人標準的速度指令格式（linear x/y/z + angular x/y/z）
#include "geometry_msgs/msg/twist.hpp"
#include <chrono>

using namespace std::chrono_literals;  // 啟用 500ms 這種字面量寫法

// 在 ROS 2 中，「節點 (Node)」= 一個獨立的執行單位，類比 MQTT 中
// 一個 client。寫一個 ROS 程式的標準模式就是繼承 rclcpp::Node。
class AutoDriveNode : public rclcpp::Node
{
public:
    // 建構子初始化 Node 基底類別並命名 "auto_drive_node"。
    // 這個名字會出現在 ros2 node list、rqt_graph、log 等所有工具中——
    // 等同 MQTT client_id，必須在系統內唯一。
    AutoDriveNode() : Node("auto_drive_node")
    {
        // 建立 Publisher：類比 MQTT 的 client.publish() 但這裡是「先註冊一個發送通道」。
        // - <Twist>：訊息型別（強型別，不像 MQTT 是 raw bytes）
        // - "cmd_vel"：topic 名稱（用相對名稱，執行時可由 launch / CLI remap 重新對應）
        // - 10：佇列深度（類比 MQTT QoS 1 的 in-flight 上限）。傳輸快但訂閱方慢時會丟舊訊息。
        publisher_ = this->create_publisher<geometry_msgs::msg::Twist>("cmd_vel", 10);

        // create_wall_timer：Node 內建的計時器，每 500ms 觸發 timer_callback。
        // 注意這是「掛在 Node 上的事件」，不是獨立 thread——它會在
        // rclcpp::spin() 的事件迴圈裡被排程執行（單執行緒模型）。
        timer_ = this->create_wall_timer(
            500ms, std::bind(&AutoDriveNode::timer_callback, this));

        // this->now() 是 ROS 時鐘（不是 std::chrono::system_clock）。
        // 在模擬器中會用模擬時間，實機中是系統時間。
        start_time_ = this->now();
    }

private:
    void timer_callback()
    {
        // 訊息物件用 default constructor 建立後再填欄位——這是 ROS 2 慣用法，
        // 因為 IDL 產生的 struct 有很多預設零值要先初始化。
        auto msg = geometry_msgs::msg::Twist();
        auto elapsed = this->now() - start_time_;

        // 啟動後 3 秒內前進，之後停止——刻意做「會自動結束」的行為，
        // 學習階段不要寫無窮 loop 撞牆。
        if (elapsed.seconds() < 3.0) {
            msg.linear.x = 0.2;   // 前進速度 0.2 m/s（小數字，模擬器才看得清楚）
            msg.angular.z = 0.0;  // 不轉向（angular.z 是 yaw 角速度）
        } else {
            msg.linear.x = 0.0;
            msg.angular.z = 0.0;
        }
        // 真正把訊息推進 ROS 通訊層，所有訂閱者會收到。
        // 沒有訂閱者時不會錯，類比 MQTT publish 到沒人訂的 topic。
        publisher_->publish(msg);
    }

    // Publisher 用 SharedPtr 持有：rclcpp 內部對 publisher 物件做 reference counting，
    // 不能用 raw pointer 或 unique_ptr。Subscriber、Service、Timer 同理。
    rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr publisher_;
    rclcpp::TimerBase::SharedPtr timer_;
    rclcpp::Time start_time_;
};

int main(int argc, char * argv[])
{
    // 初始化 ROS 2 的 context（DDS middleware、signal handler 等）。
    // argc/argv 必須傳入，因為 ROS 會解析其中的 --ros-args 參數（例如 remap）。
    rclcpp::init(argc, argv);

    // spin 是事件迴圈：阻塞當前 thread，持續處理該 Node 的 callback（timer、subscription、service）
    // 直到收到 SIGINT/SIGTERM。類比 MQTT client 的 loop_forever()。
    rclcpp::spin(std::make_shared<AutoDriveNode>());

    // 收到關閉訊號後做清理（中斷 publisher、釋放 DDS 資源）。
    rclcpp::shutdown();
    return 0;
}
