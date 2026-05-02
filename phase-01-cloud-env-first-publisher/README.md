# Phase 01：雲端環境 + 第一支 Publisher

**學完你會**：建好 ROS 2 開發環境、用 colcon 編譯一個 C++ 套件、寫一支 Publisher 讓模擬器中的車子前進 3 秒後停下。

**前置**：C++ 基礎、Linux terminal 操作。**不需要**裝任何東西。

**產出**：[`code/my_cpp_pkg/`](code/my_cpp_pkg/) — 完整可編譯的 ROS 2 套件，含 `auto_drive` 執行檔。

---

## 📍 課前觀念建立

如果你懂 Linux 和 C++，請這樣理解 ROS 2：

1. **它不是「作業系統」**：是一個建構在 Linux（通常是 Ubuntu）上的中介軟體與通訊框架。
2. **核心是「通訊」**：把機器人想像成一個系統，裡面有很多獨立的 C++ 程式（**Node**）。看影像、控制輪子、避障各是一個 Node。Node 之間靠 **Topic** 發訊息。
3. **colcon 是進階版的 make**：ROS 2 的編譯管理工具，底層仍呼叫 CMake。

---

## 🛠️ 步驟 1：架設免安裝的雲端實驗室

使用 [The Construct](https://app.theconstructsim.com/) 的 **ROSjects**——本質上是已預裝 ROS 2 + Gazebo 的免費雲端虛擬機。

1. 註冊後 → **ROSjects** → **Create New ROSject**
2. ROS Distro 選 **ROS 2 Humble**
3. 建立完成後點擊 `</> Open` 進入虛擬機
4. 介面三大核心：
   - **Terminal**：Linux 終端機
   - **Code Editor**：VS Code 風格編輯器
   - **Gazebo**：3D 模擬視窗

---

## 🕵️ 步驟 2：終端機偵探課

打開終端機與 Gazebo。假設環境內已載入一台機器人（如 OriginBot 賽車）。

### 找出通訊頻道

```bash
ros2 topic list
```

你會看到一長串列表，控制移動的頻道通常叫 `/cmd_vel`。如果場地有多台車，名稱可能加前綴，例如 `/originbot_1/cmd_vel`。

### 鍵盤遙控與 Remapping

預設鍵盤工具發送到 `/cmd_vel`，但車子可能訂閱 `/originbot_1/cmd_vel`。**不需要改程式碼**——執行時動態重映射：

```bash
ros2 run teleop_twist_keyboard teleop_twist_keyboard \
  --ros-args -r cmd_vel:=/originbot_1/cmd_vel
```

點擊終端機保持游標閃爍，按 `I, J, K, L` 操控車子。

---

## 💻 步驟 3：建立第一個 C++ 套件

### ⚠️ 新手避坑：絕對不要手動建資料夾！

ROS 2 套件必須包含 `package.xml` 和 `CMakeLists.txt`。請用 CLI：

```bash
cd ~/ros2_ws/src
ros2 pkg create --build-type ament_cmake my_cpp_pkg --dependencies rclcpp
```

---

## 步驟 4：撰寫 Publisher 節點

打開 Code Editor，到 `~/ros2_ws/src/my_cpp_pkg/src/`，新增 `auto_drive.cpp`：

> 完整檔案見 [`code/my_cpp_pkg/src/auto_drive.cpp`](code/my_cpp_pkg/src/auto_drive.cpp)

```cpp
#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/twist.hpp"
#include <chrono>

using namespace std::chrono_literals;

class AutoDriveNode : public rclcpp::Node {
public:
    AutoDriveNode() : Node("auto_drive_node") {
        // ✅ 用相對名稱 "cmd_vel"，執行時用 remapping 對應實際頻道
        publisher_ = this->create_publisher<geometry_msgs::msg::Twist>("cmd_vel", 10);

        timer_ = this->create_wall_timer(
            500ms, std::bind(&AutoDriveNode::timer_callback, this));
        start_time_ = this->now();
    }

private:
    void timer_callback() {
        auto msg = geometry_msgs::msg::Twist();
        auto elapsed = this->now() - start_time_;

        if (elapsed.seconds() < 3.0) {
            msg.linear.x = 0.2;
            msg.angular.z = 0.0;
        } else {
            msg.linear.x = 0.0;
            msg.angular.z = 0.0;
        }
        publisher_->publish(msg);
    }

    rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr publisher_;
    rclcpp::TimerBase::SharedPtr timer_;
    rclcpp::Time start_time_;
};

int main(int argc, char * argv[]) {
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<AutoDriveNode>());
    rclcpp::shutdown();
    return 0;
}
```

> **與原筆記的差異**：原版把 topic 寫死成 `/originbot_1/cmd_vel`。**這是反模式**——程式碼應該用相對名稱 `cmd_vel`，由 launch 或 CLI 的 `-r` 做 remapping。這樣同一個程式可以對任何車子使用。

---

## 步驟 5：設定 CMakeLists.txt

在 `find_package(rclcpp REQUIRED)` 之後加：

```cmake
find_package(geometry_msgs REQUIRED)

add_executable(auto_drive src/auto_drive.cpp)
ament_target_dependencies(auto_drive rclcpp geometry_msgs)

install(TARGETS auto_drive
  DESTINATION lib/${PROJECT_NAME})
```

完整版見 [`code/my_cpp_pkg/CMakeLists.txt`](code/my_cpp_pkg/CMakeLists.txt)。

---

## 步驟 6：編譯與執行

```bash
cd ~/ros2_ws
colcon build --packages-select my_cpp_pkg
source install/setup.bash

# 用 remapping 對應實際 topic
ros2 run my_cpp_pkg auto_drive --ros-args -r cmd_vel:=/originbot_1/cmd_vel
```

**成功指標**：模擬器中的車子向前開 3 秒後停下。

---

## 🎯 學到的關鍵概念

- **ROS 2 套件結構**：`package.xml` + `CMakeLists.txt` + `src/`
- **Publisher 三要素**：訊息格式（`Twist`）、Topic 名稱、佇列大小
- **Timer callback**：用 `create_wall_timer` 週期觸發
- **Remapping**：寫程式時用相對名稱，執行時動態映射
- **`rclcpp::spin`**：讓節點持續運作直到收到 Ctrl+C

---

## 下一步

- 觀念補強：[Phase 02 — 通訊機制核心觀念](../phase-02-communication-concepts/)
- 進階實作：[Phase 03 — Subscriber + 光達避障](../phase-03-subscriber-lidar-brake/)
