# Phase 04：Service Server + 開關

**學完你會**：實作一個 ROS 2 Service Server，可以用 CLI 從外部下達單次指令切換功能（在這裡是開關避障）。理解 Service vs Topic 的差異。

**前置**：[Phase 03](../phase-03-subscriber-lidar-brake/) — 已能跑光達避障。

**產出**：[`code/my_cpp_pkg/`](code/my_cpp_pkg/) — 含 `auto_brake_service` 執行檔，同時是 Subscriber + Publisher + Service Server。

---

## 🧠 核心觀念：Topic vs Service

| 模式 | 比喻 | 適合的場景 |
|------|------|-----------|
| **Topic（發布/訂閱）** | 廣播電台。沒人聽也持續播。 | 感測器資料、連續控制指令 |
| **Service（一問一答）** | 打電話點餐。有請求才有回應。 | 開關功能、單次計算、拍照 |

Service 包含兩個角色：
- **Server（提供者）**：接電話的人。收到 Request → 處理 → 回 Response。
- **Client（呼叫者）**：打電話的人。送 Request → 等 Response。

本章把 Phase 03 的避障改造為 Service Server，提供外部開關。

---

## 💻 步驟 1：撰寫帶 Service 的節點

使用 ROS 2 內建的 `std_srvs/srv/SetBool`：Request 是布林、Response 是 `success`+`message`。

完整程式見 [`code/my_cpp_pkg/src/auto_brake_service.cpp`](code/my_cpp_pkg/src/auto_brake_service.cpp)：

```cpp
#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/twist.hpp"
#include "sensor_msgs/msg/point_cloud2.hpp"
#include "sensor_msgs/point_cloud2_iterator.hpp"
#include "std_srvs/srv/set_bool.hpp"
#include <atomic>

using std::placeholders::_1;
using std::placeholders::_2;

class AutoBrakeServiceNode : public rclcpp::Node
{
public:
    AutoBrakeServiceNode() : Node("auto_brake_service_node")
    {
        publisher_ = this->create_publisher<geometry_msgs::msg::Twist>("cmd_vel", 10);

        subscriber_ = this->create_subscription<sensor_msgs::msg::PointCloud2>(
            "lidar_points", rclcpp::SensorDataQoS(),
            std::bind(&AutoBrakeServiceNode::cloud_callback, this, _1));

        // 建立 Service Server
        service_ = this->create_service<std_srvs::srv::SetBool>(
            "toggle_brake",
            std::bind(&AutoBrakeServiceNode::toggle_brake_callback, this, _1, _2));

        RCLCPP_INFO(this->get_logger(), "AEB Service ready: 'toggle_brake'");
    }

private:
    // ⚠️ 用 atomic：service callback 與 sub callback 可能在不同 thread
    std::atomic<bool> is_brake_active_{true};

    void toggle_brake_callback(
        const std::shared_ptr<std_srvs::srv::SetBool::Request> request,
        std::shared_ptr<std_srvs::srv::SetBool::Response> response)
    {
        is_brake_active_ = request->data;
        response->success = true;
        response->message = is_brake_active_
            ? "Brake system ENABLED."
            : "Brake system DISABLED. Watch out!";
        RCLCPP_WARN(this->get_logger(), ">>> Service: %s <<<",
                    is_brake_active_ ? "ENABLED" : "DISABLED");
    }

    void cloud_callback(const sensor_msgs::msg::PointCloud2::SharedPtr msg)
    {
        if (!is_brake_active_) {
            RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 2000,
                "Brake system offline. Waiting for enable command...");
            return;
        }

        auto twist_msg = geometry_msgs::msg::Twist();
        float min_forward_distance = 100.0f;
        sensor_msgs::PointCloud2ConstIterator<float> iter_x(*msg, "x");
        sensor_msgs::PointCloud2ConstIterator<float> iter_y(*msg, "y");

        for (; iter_x != iter_x.end(); ++iter_x, ++iter_y) {
            float x = *iter_x;
            float y = *iter_y;
            if (x > 0.0f && std::abs(y) < 0.2f) {
                if (x < min_forward_distance) min_forward_distance = x;
            }
        }

        if (min_forward_distance > 1.0f) {
            twist_msg.linear.x = 0.2;
        } else {
            twist_msg.linear.x = 0.0;
            RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 1000,
                "Obstacle at %.2fm! BRAKING!", min_forward_distance);
        }
        publisher_->publish(twist_msg);
    }

    rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr publisher_;
    rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::SharedPtr subscriber_;
    rclcpp::Service<std_srvs::srv::SetBool>::SharedPtr service_;
};

int main(int argc, char * argv[])
{
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<AutoBrakeServiceNode>());
    rclcpp::shutdown();
    return 0;
}
```

> **與原筆記的差異**：
> 1. Topic 用相對名稱（`cmd_vel`、`lidar_points`），由 remapping 對應實際頻道。
> 2. `is_brake_active_` 改成 `std::atomic<bool>`。**為什麼**：service callback 與 sub callback 在多執行緒 Executor 下可能在不同 thread，普通 `bool` 會造成 race condition。`atomic` 是零成本的正確解。Phase 06.5 會深入這塊。

---

## 步驟 2：CMakeLists.txt 與 package.xml

完整版見 [`code/my_cpp_pkg/CMakeLists.txt`](code/my_cpp_pkg/CMakeLists.txt) 與 [`code/my_cpp_pkg/package.xml`](code/my_cpp_pkg/package.xml)。

關鍵新增：

```cmake
find_package(std_srvs REQUIRED)

add_executable(auto_brake_service src/auto_brake_service.cpp)
ament_target_dependencies(auto_brake_service
  rclcpp geometry_msgs sensor_msgs std_srvs)
```

```xml
<depend>std_srvs</depend>
```

---

## 🚀 步驟 3：雙終端機實戰測試

> 兩種環境的差異只在「remap 到哪個 topic」。完整環境比較見 [SETUP.md](../SETUP.md)。
> 本機 turtlebot3 預設無 PointCloud2，需參考 [Phase 03](../phase-03-subscriber-lidar-brake/) 的兩個做法之一。

### 編譯（兩種環境通用）

```bash
cd ~/ros2_ws
colcon build --packages-select my_cpp_pkg
source install/setup.bash
```

### ☁️ TheConstructSim — 終端機 1：啟動 Server

```bash
ros2 run my_cpp_pkg auto_brake_service --ros-args \
  -r cmd_vel:=/originbot_1/cmd_vel \
  -r lidar_points:=/livox/lidar
```

### 💻 本機 WSL2 — 終端機 1：啟動 Server

```bash
# 假設你已用 Phase 03 做法 2 起好 turtlebot3 waffle + Gazebo
ros2 run my_cpp_pkg auto_brake_service --ros-args \
  -r lidar_points:=/intel_realsense_r200_depth/points
```

### 終端機 2：呼叫 Service 關閉避障（兩種環境通用）

```bash
ros2 service call /toggle_brake std_srvs/srv/SetBool "{data: false}"
```

要重新啟動避障，把 `false` 改成 `true` 再呼叫一次。

> 💡 進階：用 `rqt_service_caller`（GUI 版本）也可以呼叫，下一階段 Phase 05 會教。

---

## 📊 系統日誌解讀

成功後會看到三個情境清楚展示 Topic（持續）vs Service（突發）：

### 情境 1：正常避障（System ENABLED）
```
[WARN] [auto_brake_service_node]: Obstacle at 0.87m! BRAKING!
[WARN] [auto_brake_service_node]: Obstacle at 0.87m! BRAKING!
```
`is_brake_active_` = true，光達持續觸發 callback，發送速度 0.0。

### 情境 2：呼叫 Service 的瞬間
```
[WARN] [auto_brake_service_node]: >>> Service: DISABLED <<<
```
從另一終端機 `ros2 service call` 的那一刻，觸發 `toggle_brake_callback`。

### 情境 3：休眠模式（System DISABLED）
```
[INFO] [auto_brake_service_node]: Brake system offline. Waiting for enable command...
[INFO] [auto_brake_service_node]: Brake system offline. Waiting for enable command...
```
光達資料仍持續進來（`cloud_callback` 仍被觸發），但 `is_brake_active_` = false 直接 return，不發任何指令。

---

## 🎯 學到的關鍵概念

- **Service = Request-Response 一問一答**，與 Topic 的廣播模式對立
- **Service callback 簽章**：`(Request, Response)`，回傳值寫進 `Response`
- **多 callback 共享狀態要小心**：用 `std::atomic` 或 mutex
- **CLI 呼叫格式**：`ros2 service call <name> <type> "<data_yaml>"`

---

## 下一步

下一章開始學 **Parameters** — 讓常數（如 `0.2 m/s`、`1.0 m`）可以從外部動態調整，免重新編譯。
- Phase 05 — Parameters（待完成）
