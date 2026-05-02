# Phase 03：Subscriber + 光達避障

**學完你會**：寫一個既訂閱（聽光達）又發布（送速度指令）的節點，讓車子偵測到前方障礙物時自動煞車。同時掌握 ROS 2 的 QoS 概念。

**前置**：[Phase 01](../phase-01-cloud-env-first-publisher/) 與 [Phase 02](../phase-02-communication-concepts/)。

**產出**：[`code/my_cpp_pkg/`](code/my_cpp_pkg/) — 可獨立編譯的套件，含 `auto_brake` 執行檔。

---

## 👁️ 核心觀念：機器人的眼睛 (LiDAR)

光達（LiDAR）像蝙蝠的超音波，發射雷射並接收反射來測量距離。ROS 2 中常見兩種訊息格式：

| 格式 | 維度 | 用途 |
|------|------|------|
| `sensor_msgs/msg/LaserScan` | 2D | 平面陣列，依角度紀錄距離。簡單。 |
| `sensor_msgs/msg/PointCloud2` | 3D | 成千上萬個 (X, Y, Z) 點。複雜但能看立體空間。 |

本章使用配備 Livox 3D 光達的 OriginBot，資料頻道為 `/livox/lidar`，格式為 `PointCloud2`。

---

## 🕵️ 步驟 1：終端機偵探課（必做）

寫程式前先排查頻道狀態，避開 90% 的「靜默失敗」。

```bash
# 1. 確認光達有在發送（檢查心跳）
ros2 topic hz /livox/lidar

# 2. 確認資料格式
ros2 topic info /livox/lidar
```

預期看到 Type 是 `sensor_msgs/msg/PointCloud2`。這決定 C++ 程式要 include 的標頭檔。

---

## ⚠️ 關鍵知識：QoS（Quality of Service）

ROS 2 的 Publisher 和 Subscriber 之間必須**達成 QoS 協議**才能成功通訊。如果不匹配，訂閱會「靜默失敗」（沒有錯誤訊息，但收不到資料）。

| 場景 | 預設 QoS | 一致性 |
|------|----------|--------|
| 一般訊息（如 `cmd_vel`） | **Reliable**（可靠） | 不丟封包 |
| 感測器訊息（光達、相機） | **Best Effort**（盡力而為） | 允許丟封包，換取低延遲 |

**規則**：訂閱感測器資料**必須**用 `rclcpp::SensorDataQoS()`，否則收不到。

---

## 💻 步驟 2：撰寫 3D 點雲自動煞車節點

**任務**：監聽 `/livox/lidar`，解析正前方 40 公分寬走廊內的點雲。最近的點 < 1.0 m 就煞車，否則維持 0.2 m/s 前進。

完整程式見 [`code/my_cpp_pkg/src/auto_brake.cpp`](code/my_cpp_pkg/src/auto_brake.cpp)：

```cpp
#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/twist.hpp"
#include "sensor_msgs/msg/point_cloud2.hpp"
#include "sensor_msgs/point_cloud2_iterator.hpp"

using std::placeholders::_1;

class AutoBrakeNode : public rclcpp::Node
{
public:
    AutoBrakeNode() : Node("auto_brake_node")
    {
        publisher_ = this->create_publisher<geometry_msgs::msg::Twist>("cmd_vel", 10);

        // ⚠️ 感測器一定要用 SensorDataQoS()
        subscriber_ = this->create_subscription<sensor_msgs::msg::PointCloud2>(
            "lidar_points", rclcpp::SensorDataQoS(),
            std::bind(&AutoBrakeNode::cloud_callback, this, _1));

        RCLCPP_INFO(this->get_logger(), "3D Auto Brake Started!");
    }

private:
    void cloud_callback(const sensor_msgs::msg::PointCloud2::SharedPtr msg)
    {
        auto twist_msg = geometry_msgs::msg::Twist();
        float min_forward_distance = 100.0f;

        sensor_msgs::PointCloud2ConstIterator<float> iter_x(*msg, "x");
        sensor_msgs::PointCloud2ConstIterator<float> iter_y(*msg, "y");

        for (; iter_x != iter_x.end(); ++iter_x, ++iter_y) {
            float x = *iter_x;
            float y = *iter_y;

            // 篩選正前方 40cm 寬走廊
            if (x > 0.0f && std::abs(y) < 0.2f) {
                if (x < min_forward_distance) min_forward_distance = x;
            }
        }

        if (min_forward_distance > 1.0f) {
            twist_msg.linear.x = 0.2;
            RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 1000,
                "Clear ahead (Closest: %.2fm)", min_forward_distance);
        } else {
            twist_msg.linear.x = 0.0;
            RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 1000,
                "Obstacle at %.2fm! BRAKING!", min_forward_distance);
        }

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
```

> **與原筆記的差異**：
> 1. Topic 用相對名稱 `cmd_vel`、`lidar_points`，由執行時 remapping 對應實際 topic（不寫死）。
> 2. `RCLCPP_*_THROTTLE`：每秒最多印一次 log，避免 spam。

---

## 步驟 3：CMakeLists.txt 與 package.xml

完整版見 [`code/my_cpp_pkg/CMakeLists.txt`](code/my_cpp_pkg/CMakeLists.txt) 與 [`code/my_cpp_pkg/package.xml`](code/my_cpp_pkg/package.xml)。

關鍵新增：

```cmake
find_package(sensor_msgs REQUIRED)

add_executable(auto_brake src/auto_brake.cpp)
ament_target_dependencies(auto_brake rclcpp geometry_msgs sensor_msgs)
```

```xml
<depend>sensor_msgs</depend>
```

---

## 步驟 4：編譯與執行

```bash
cd ~/ros2_ws
colcon build --packages-select my_cpp_pkg
source install/setup.bash

# 用 remapping 對應實際的 topic
ros2 run my_cpp_pkg auto_brake --ros-args \
  -r cmd_vel:=/originbot_1/cmd_vel \
  -r lidar_points:=/livox/lidar
```

---

## 🎉 成功指標

- 終端機印出車子前進中的距離資訊。
- 接近牆壁（< 1.0m）時 terminal 顯示黃色 `[WARN]`，車子煞車。

---

## 🌟 挑戰

修改 `auto_brake.cpp`，偵測到障礙物時不要原地煞車，而是 `twist_msg.angular.z = 0.5` 自動轉彎，做出真正的「避障巡邏」。

---

## 下一步

學會了 Topic 雙向通訊。但 Topic 是「持續廣播」，如果想下達「單次且需要確認」的指令（如開關功能）呢？
- [Phase 04 — Services 開關](../phase-04-services-toggle/)
