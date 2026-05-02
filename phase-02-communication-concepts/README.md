# Phase 02：ROS 2 通訊機制核心觀念

**學完你會**：理解 Node / Topic / Message / Pub-Sub 模型，能拆解任何一段 ROS 2 程式碼。

**前置**：[Phase 01](../phase-01-cloud-env-first-publisher/) — 至少跑過一次 `auto_drive`。

**產出**：純觀念章節，無新 code。回頭逐行拆解 Phase 01 的 `auto_drive.cpp`。

---

## 📡 核心觀念：ROS 2 的「廣播電台」模型

學 ROS 2 時，請先忘掉「A 函數呼叫 B 函數」的傳統思維。把 ROS 2 想像成**大型廣播系統**或 **YouTube 訂閱機制**。

### 1. 節點 (Node)

獨立運作的程式。為了避免一個 bug 拖垮整台車，工作會拆成多個 Node。
- 一個 Node 看攝影機
- 一個 Node 控制馬達
- 一個 Node 做避障

Phase 01 的 `auto_drive` 就是一個「發出移動指令」的 Node。

### 2. 主題 (Topic)

Node 之間溝通的**頻道**。Node 不需要知道對方是誰，只需要往特定頻道丟資料，或聽特定頻道。

範例：`/originbot_1/cmd_vel` 是專門傳遞「速度指令」的頻道。

### 3. 訊息 (Message / msg)

頻道裡流通的**資料格式**。不同頻道用不同格式，就像收音機傳聲音、電視機傳影像。

範例：速度指令的格式叫 `geometry_msgs/msg/Twist`，內含「線性速度 (linear)」和「角速度 (angular)」。

### 4. 發布者 (Publisher) 與 訂閱者 (Subscriber)

| 角色 | 比喻 | Phase 01 中的對應 |
|------|------|------------------|
| **Publisher** | 在頻道講話的人 | 你寫的 `auto_drive` |
| **Subscriber** | 在頻道聽話的人 | 賽車底層驅動，聽到指令就轉馬達 |

---

## 🔄 架構示意圖

```
[你的 C++ 程式]                       [賽車底層驅動]
 (Publisher)                          (Subscriber)
      │                                    ▲
      │ 發布 Twist 訊息 (前進 0.2m/s)        │ 接收 Twist 並轉動馬達
      ▼                                    │
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    Topic 頻道: /cmd_vel
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
```

---

## 💻 回頭拆解 Phase 01 的 `auto_drive.cpp`

帶著上面的觀念回看 code，會發現一切都很合理。

### 1. 引入工具包

```cpp
#include "rclcpp/rclcpp.hpp"             // ROS 2 C++ 核心 (含 Node 功能)
#include "geometry_msgs/msg/twist.hpp"   // Twist 訊息格式
#include <chrono>

using namespace std::chrono_literals;    // 讓 500ms 這種寫法可用
```

### 2. 建立節點

```cpp
// 繼承 rclcpp::Node，這個 class 就是一個標準 ROS 2 節點
class AutoDriveNode : public rclcpp::Node
{
public:
    AutoDriveNode() : Node("auto_drive_node")  // 節點命名
    {
```

### 3. 註冊 Publisher 與 Timer

```cpp
        // [資料格式] [頻道名稱] [佇列大小]
        publisher_ = this->create_publisher<geometry_msgs::msg::Twist>("cmd_vel", 10);

        // 每 500ms 自動觸發 timer_callback
        timer_ = this->create_wall_timer(
            500ms, std::bind(&AutoDriveNode::timer_callback, this));

        start_time_ = this->now();
    }
```

### 4. 大腦：Callback 函數

```cpp
private:
    void timer_callback()
    {
        auto msg = geometry_msgs::msg::Twist();
        auto elapsed = this->now() - start_time_;

        if (elapsed.seconds() < 3.0) {
            msg.linear.x = 0.2;   // 前進 0.2 m/s
            msg.angular.z = 0.0;  // 不轉彎
        } else {
            msg.linear.x = 0.0;   // 煞車
            msg.angular.z = 0.0;
        }

        publisher_->publish(msg); // 把訊息丟到頻道上
    }
```

### 5. 啟動引擎

```cpp
int main(int argc, char * argv[])
{
    rclcpp::init(argc, argv);                        // 初始化 ROS 2
    rclcpp::spin(std::make_shared<AutoDriveNode>()); // 卡死在這裡持續運作
    rclcpp::shutdown();                              // 安全關閉
    return 0;
}
```

> **`rclcpp::spin` 在做什麼**：把節點「跑起來」，並阻塞 main thread。Timer 才能不斷被觸發、callback 才會被呼叫，直到 Ctrl+C。

---

## 🎯 學到的關鍵概念

- ROS 2 = 訊息匯流排，不是函數呼叫
- 節點之間靠 **Topic** 解耦，發送方不需要知道接收方是誰
- 一個 Topic 必須對應一種固定的 Message 格式
- Publisher / Subscriber 是兩個對稱的 API

---

## 下一步

學會了「說話」（Publish），接下來學「聽話」：
- [Phase 03 — Subscriber + 光達避障](../phase-03-subscriber-lidar-brake/)
