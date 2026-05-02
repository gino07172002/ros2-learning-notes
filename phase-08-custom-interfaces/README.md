# Phase 08：Custom Interfaces — 自己定義訊息協議

> Part 3 第一章：**核心分水嶺**。從這章開始你不再只是「使用」ROS 通訊，而是「設計」自己的協議。

**學完你會**：寫 .msg/.srv/.action 三種自訂型別、建一個獨立的 interface 套件、在 C++ Node 用自訂型別、知道為什麼業界把 interface 套件分離、了解 rosidl 從文字檔生成 C++ class 的流程。

**前置**：
- [Phase 07 Mini Capstone](../phase-07-mini-capstone-1/) — 你會把它升級成 v2
- 對 Subscriber + Service 的 callback 簽章熟悉

**產出**：
- [`code/my_robot_interfaces/`](code/my_robot_interfaces/) — 純 .msg/.srv/.action 定義包
- [`code/my_cpp_pkg/`](code/my_cpp_pkg/) — 用自訂型別的 smart_brake_v2 + approach_client

**環境**：☁️ TheConstructSim + 💻 本機 WSL 雙環境通用。本機用 fake_lidar 模擬，雲端可直接接 OriginBot 真模擬光達。

---

## 為什麼這章是「真正的核心」

Phase 01–07 都用了**現成的 ROS 訊息類型**：
- `geometry_msgs/Twist`、`sensor_msgs/PointCloud2`、`std_srvs/SetBool`...

這些是社群預先設計給通用機器人用的。**你只是消費者**。

Phase 08 開始，**你是設計者**：
- 你的系統有獨特的領域名詞 → 寫自己的 .msg
- 你的服務需要傳多個欄位（不是只有 bool）→ 寫自己的 .srv
- 你的任務需要長時間執行 + 進度回報 → 寫自己的 .action

業界 ROS 系統 90% 都有自己的 interface 套件。**會寫 = 你能跟 Nav2/MoveIt/任何開源 ROS 系統一樣設計專業介面**。

---

## 三個檔案類型對照

| 檔案 | 用途 | 對應通訊 | 結構 |
|------|------|---------|------|
| `.msg` | 資料結構 | Topic | 一段欄位 |
| `.srv` | Request + Response | Service | 兩段欄位（用 `---` 分隔）|
| `.action` | Goal + Result + Feedback | Action | 三段欄位（兩個 `---` 分隔）|

---

## 🏗️ 業界慣例：interface 套件獨立

**錯的做法**（新手常做）：
```
my_robot_pkg/                ← 一個套件包山包海
├── package.xml
├── msg/MyMsg.msg
└── src/my_node.cpp          ← C++ 跟 .msg 混在一起
```

**對的做法**（業界慣例）：
```
my_robot_interfaces/         ← 純定義包，沒有可執行檔
├── msg/BrakeStatus.msg
├── srv/SetBrakeMode.srv
└── action/Approach.action

my_cpp_pkg/                  ← 邏輯包，依賴 interfaces
├── src/smart_brake_v2.cpp
└── src/approach_client.cpp

my_python_pkg/               ← 視覺化包，也依賴 interfaces
└── ...
```

**為什麼分離**：
- **多語言共用**：C++ 跟 Python 套件依賴同一份 .msg，不會有「兩邊定義不一致」
- **多團隊並行**：感知團隊、控制團隊、UI 團隊各寫各的，只要對齊 .msg 合約
- **編譯加速**：interface 套件改動少，邏輯套件可以單獨 rebuild

---

## 📝 三個自訂 interface

### `BrakeStatus.msg` — Topic 廣播

完整檔案見 [`code/my_robot_interfaces/msg/BrakeStatus.msg`](code/my_robot_interfaces/msg/BrakeStatus.msg)：

```
# 常數（enum 慣例：大寫 + 等號）
uint8 MODE_DISABLED=0
uint8 MODE_ENABLED=1
uint8 MODE_EMERGENCY=2

# 欄位（type space name）
std_msgs/Header header           # 巢狀型別 — 含 stamp + frame_id
uint8 mode
float32 current_speed
float32 closest_obstacle_distance
float32 uptime_seconds
string status_text
```

**重點**：
- 常數會自動生成 `BrakeStatus::MODE_ENABLED` 之類的 C++ static const
- `std_msgs/Header` 是巢狀型別，必須在 CMake 與 package.xml 宣告依賴
- 註解放欄位「上方」獨立一行，**不能放欄位後面**

### `SetBrakeMode.srv` — Service 一問一答

完整檔案見 [`code/my_robot_interfaces/srv/SetBrakeMode.srv`](code/my_robot_interfaces/srv/SetBrakeMode.srv)：

```
# Request
uint8 mode
float32 max_speed       # < 0 表示「不改變」
string reason
---
# Response
bool success
uint8 previous_mode     # 給 client 看「我改了什麼」
float32 applied_max_speed
string message
```

**比 `std_srvs/SetBool` 強的地方**：
- Request 一次傳 3 個欄位（不是只有 bool）
- Response 回傳「之前是什麼」+「之後是什麼」，方便 client 做 diff log

### `Approach.action` — Action 長任務

完整檔案見 [`code/my_robot_interfaces/action/Approach.action`](code/my_robot_interfaces/action/Approach.action)：

```
# Goal: client 開始時送
float32 target_distance
float32 approach_speed
---
# Result: 任務結束時 server 送
bool success
float32 final_distance
float32 elapsed_seconds
---
# Feedback: server 持續送進度
float32 current_distance
float32 elapsed_seconds
string status
```

**Action 是 ROS 最強的通訊機制**：
- 比 Topic 更精確（client 知道任務何時開始/結束）
- 比 Service 更靈活（中途可以取消、可以收進度）
- 適合長時間 + 異步 + 可中止的任務（導航、機械臂運動規劃）

---

## ⚙️ Interface 套件的 CMake 設定（最容易踩雷）

完整見 [`code/my_robot_interfaces/CMakeLists.txt`](code/my_robot_interfaces/CMakeLists.txt)：

```cmake
find_package(rosidl_default_generators REQUIRED)
find_package(std_msgs REQUIRED)            # 因為用了 std_msgs/Header

rosidl_generate_interfaces(${PROJECT_NAME}
  "msg/BrakeStatus.msg"
  "srv/SetBrakeMode.srv"
  "action/Approach.action"
  DEPENDENCIES std_msgs                     # 巢狀型別的依賴必須在這宣告
)

ament_export_dependencies(rosidl_default_runtime)
```

`package.xml` 也有三個必備宣告：

```xml
<buildtool_depend>rosidl_default_generators</buildtool_depend>
<exec_depend>rosidl_default_runtime</exec_depend>
<member_of_group>rosidl_interface_packages</member_of_group>
<depend>std_msgs</depend>
```

> ⚠️ **新手大坑**：`<member_of_group>rosidl_interface_packages</member_of_group>` 漏寫，build 會過但**下游套件 find_package(...) 找不到生成的型別**。網上很多 tutorial 沒提這行。

---

## 💻 在 C++ Node 用自訂 interface

完整見 [`code/my_cpp_pkg/src/smart_brake_v2.cpp`](code/my_cpp_pkg/src/smart_brake_v2.cpp)。

### 引入標頭檔

```cpp
// 路徑慣例：<package>/<msg|srv|action>/<snake_case_filename>.hpp
// PascalCase 的 msg 名 → 自動轉 snake_case 檔名
#include "my_robot_interfaces/msg/brake_status.hpp"      // BrakeStatus.msg
#include "my_robot_interfaces/srv/set_brake_mode.hpp"    // SetBrakeMode.srv
#include "my_robot_interfaces/action/approach.hpp"       // Approach.action
```

### 用 IDL 生成的常數

```cpp
using BrakeStatus = my_robot_interfaces::msg::BrakeStatus;

// 直接用常數，不用 magic number
mode_ = BrakeStatus::MODE_ENABLED;
if (mode == BrakeStatus::MODE_EMERGENCY) { ... }
```

### Publisher / Service / Action 寫法跟標準型別一樣

```cpp
// Publisher - template 參數用自訂型別
status_pub_ = create_publisher<BrakeStatus>("brake_status", 10);

// Service Server
mode_service_ = create_service<SetBrakeMode>(
    "set_brake_mode",
    std::bind(&SmartBrakeV2::set_mode_callback, this, _1, _2));

// Action Server (要 #include "rclcpp_action/rclcpp_action.hpp")
approach_server_ = rclcpp_action::create_server<Approach>(
    this, "approach",
    std::bind(&SmartBrakeV2::handle_goal, this, _1, _2),
    std::bind(&SmartBrakeV2::handle_cancel, this, _1),
    std::bind(&SmartBrakeV2::handle_accepted, this, _1));
```

### 下游套件的 CMake 必須宣告依賴

```cmake
find_package(my_robot_interfaces REQUIRED)
find_package(rclcpp_action REQUIRED)        # Action 才需要

ament_target_dependencies(smart_brake_v2
  rclcpp rclcpp_action geometry_msgs sensor_msgs my_robot_interfaces)
```

---

## 🚀 完整 Demo 流程

> 兩種環境的差異：
> - **本機 WSL**：用 `cp` 把套件搬進 `~/ros2_ws/src/`，光達訊息靠 `fake_lidar.py` 產生
> - **TheConstructSim**：用 web Code Editor 直接建套件、或 `git clone` 拉 repo；有真模擬光達 `/livox/lidar` 不需要 fake_lidar
>
> 完整環境差異見 [SETUP.md](../SETUP.md)。

### Step 1：把套件放進工作區

#### ☁️ TheConstructSim

進到 ROSject 後，**用 git clone** 最方便：

```bash
cd ~/ros2_ws/src
git clone https://github.com/gino07172002/ros2-learning-notes.git
# 把 Phase 08 的兩個套件 symlink / 複製進來
ln -s ros2-learning-notes/phase-08-custom-interfaces/code/my_robot_interfaces .
ln -s ros2-learning-notes/phase-08-custom-interfaces/code/my_cpp_pkg .
```

或用 ROSject 內建的 **Code Editor** 手動建立檔案——優點是邊編輯邊跑，缺點是要逐一複製貼上。

#### 💻 本機 WSL2

```bash
cp -r /mnt/d/ros_learn/ros2-learning-notes/phase-08-custom-interfaces/code/my_robot_interfaces \
      ~/ros2_ws/src/

cp -r /mnt/d/ros_learn/ros2-learning-notes/phase-08-custom-interfaces/code/my_cpp_pkg \
      ~/ros2_ws/src/phase08_pkg

# 改名避免跟其他 phase 撞名
sed -i 's|<name>my_cpp_pkg</name>|<name>phase08_pkg</name>|' ~/ros2_ws/src/phase08_pkg/package.xml
sed -i 's|project(my_cpp_pkg)|project(phase08_pkg)|' ~/ros2_ws/src/phase08_pkg/CMakeLists.txt
```

> 💡 為什麼本機要改名：本機工作區同時放了 phase01–07 全部套件，避免 colcon 看到兩個 `my_cpp_pkg` 衝突。雲端環境每次重新建一個 ROSject 就乾淨，不用改名。

### Step 2：build interface 套件（**順序很重要**）

兩種環境通用：

```bash
cd ~/ros2_ws
colcon build --packages-select my_robot_interfaces
source install/setup.bash
```

> ⚠️ **必須先 build 這個再 build 邏輯套件**——下游 `phase08_pkg` 用到生成的 .hpp 標頭檔，順序不對會找不到。

驗證 ROS 看得到型別：

```bash
ros2 interface list | grep my_robot
# my_robot_interfaces/msg/BrakeStatus
# my_robot_interfaces/srv/SetBrakeMode
# my_robot_interfaces/action/Approach

ros2 interface show my_robot_interfaces/msg/BrakeStatus
```

> 🎯 看到 `ros2 interface show` 把你寫的 .msg 完整印出來、且 `std_msgs/Header` 自動展開成 `builtin_interfaces/Time stamp + string frame_id`——這就是 rosidl 工作的證明。

### Step 3：build 邏輯套件

#### ☁️ TheConstructSim
```bash
colcon build --packages-select my_cpp_pkg
source install/setup.bash
```

#### 💻 本機 WSL2
```bash
colcon build --packages-select phase08_pkg
source install/setup.bash
```

### Step 4：啟動系統

#### ☁️ TheConstructSim（用真模擬光達）

ROSject 提供的 OriginBot 場景已經在 `/livox/lidar` 持續發 PointCloud2，**不需要 fake_lidar**。

**Terminal 1**（smart_brake_v2，把訂閱改到真光達）：
```bash
ros2 run my_cpp_pkg smart_brake_v2 --ros-args \
  -r lidar_points:=/livox/lidar \
  -r cmd_vel:=/originbot_1/cmd_vel
```

> 重點：用 `--ros-args -r` 把程式裡的相對名稱 `lidar_points` 對應到場景的真實 topic `/livox/lidar`。這就是 Phase 01 教的 remap 在這裡發揮價值。

#### 💻 本機 WSL2（用 fake_lidar 製造障礙物）

**Terminal 1**（fake lidar，模擬 0.5m 前方障礙物）：
```bash
python3 ~/fake_lidar.py 0.5
```

**Terminal 2**（smart_brake_v2，預設 topic 名稱跟 fake_lidar 對齊）：
```bash
ros2 run phase08_pkg smart_brake_v2
```

> 💡 沒有真光達的本機環境必須靠 `fake_lidar.py` 製造 PointCloud2 訊息給 smart_brake_v2 訂閱。雲端因為已經有真模擬，省了這一步。

### Step 5：Demo 1 — 看自訂 Topic 廣播

> 以下 Demo 1/2/3 在兩種環境**指令完全相同**——因為 ros2 CLI 工具直接認 topic/service/action 名稱，跟你的套件名無關。差別只在最後 Demo 3 的 `ros2 run` 套件名（雲端 `my_cpp_pkg` vs 本機 `phase08_pkg`）。

新開一個 terminal：

```bash
ros2 topic echo /brake_status --once
```

預期輸出：
```yaml
header:
  stamp: {sec: ..., nanosec: ...}
  frame_id: smart_brake_v2
mode: 1                                       # MODE_ENABLED
current_speed: 0.15
closest_obstacle_distance: 0.5
uptime_seconds: 18.0
status_text: '[ENABLED] speed=0.15 obstacle=0.50m'
```

🎯 **這就是自訂 BrakeStatus 訊息的實際內容**——一個訊息塞 5 個有用欄位，比 `std_msgs/Float32` 強多了。

> 💡 **TheConstruct 上要看數值有變化**，因為光達是真模擬，車子接近障礙物時 `closest_obstacle_distance` 會逐漸減少。本機 fake_lidar 是固定值。

### Step 6：Demo 2 — 呼叫自訂 Service

兩種環境通用：

```bash
# 切換到 EMERGENCY 模式（mode=2）
ros2 service call /set_brake_mode my_robot_interfaces/srv/SetBrakeMode \
  '{mode: 2, max_speed: -1.0, reason: "testing emergency"}'
```

預期 Response：
```yaml
success: true
previous_mode: 1                              # 從 ENABLED 切換
applied_max_speed: 0.5                        # max_speed: -1 = 不改變
message: 'OK: testing emergency'
```

🎯 **比 SetBool 強**：Request 一次傳 3 個欄位、Response 回傳新舊狀態 diff、自帶 reason 訊息。

> 💡 **TheConstruct 上**：set EMERGENCY 後 OriginBot 會立刻停下（cmd_vel.linear.x = 0）。Gazebo 視窗能直接看到效果。
> **本機**：因為沒接 turtlesim，只能從 `/cmd_vel` topic 看數值變化（用 `ros2 topic echo /cmd_vel`）。

### Step 7：Demo 3 — 呼叫自訂 Action

#### ☁️ TheConstructSim
```bash
ros2 run my_cpp_pkg approach_client 0.5 0.3
```

#### 💻 本機 WSL2
```bash
ros2 run phase08_pkg approach_client 0.5 0.3
```

預期輸出：
```
[approach_client]: Goal accepted, waiting for result...
[approach_client]: [Feedback] dist=0.50m, 0.0s elapsed: Reached target
[approach_client]: ✅ SUCCESS: final_dist=0.50m in 0.0s
```

🎯 **Action 的三段（goal/feedback/result）全部運作**——goal 被接受、feedback 持續送、result 結算。Phase 13 會深入 action 的進階用法（cancel、abort、長任務）。

> 💡 **TheConstruct 上可以看到動態效果**：因為 OriginBot 真的會邊走邊靠近障礙物，feedback 會印出多筆 `current_distance` 從遠到近遞減（`1.20m → 0.95m → 0.70m → 0.50m`），最後 SUCCESS。
> **本機 fake_lidar 0.5m 固定值** 一啟動就符合 target，會看到 0.0s 即時 SUCCESS（單筆 feedback）——驗證機制正確但無法觀察「靠近過程」。

---

## 🐛 常見雷與解法

### 雷 1：`#include` 找不到自訂 .hpp
```cpp
#include "my_robot_interfaces/msg/BrakeStatus.hpp"  // ❌ 大寫
#include "my_robot_interfaces/msg/brake_status.hpp" // ✅ snake_case
```

rosidl 把 PascalCase msg 名自動轉成 snake_case 檔名。**永遠用 snake_case 引入**。

### 雷 2：colcon build 報「找不到 std_msgs/Header」
```cmake
# ❌
rosidl_generate_interfaces(${PROJECT_NAME}
  "msg/BrakeStatus.msg"
)

# ✅ 必須宣告 DEPENDENCIES
rosidl_generate_interfaces(${PROJECT_NAME}
  "msg/BrakeStatus.msg"
  DEPENDENCIES std_msgs
)
```

### 雷 3：下游套件 `find_package(my_robot_interfaces)` 找不到
package.xml 漏寫：
```xml
<member_of_group>rosidl_interface_packages</member_of_group>
```

### 雷 4：.msg 註解放錯位置
```
# ❌ 註解放欄位同一行的後面 — 會解析錯
float32 speed   # speed in m/s

# ✅ 註解放單獨一行
# speed in m/s
float32 speed
```

### 雷 5：interface 改了但下游編不過
```bash
# 必須重 build interface 套件 + source
colcon build --packages-select my_robot_interfaces
source install/setup.bash

# 然後重 build 下游
colcon build --packages-select phase08_pkg
```

下游套件用的是 install 裡的 generated headers，不是 src 裡的 .msg。

### 雷 6：rclcpp_action 沒 link
```cmake
# Action 必須單獨 find_package
find_package(rclcpp_action REQUIRED)
ament_target_dependencies(your_node
  rclcpp rclcpp_action ...)   # ← 別忘 rclcpp_action
```

### 雷 7（TheConstruct 環境特有）：套件改完沒重 source
TheConstruct 的 web terminal 開了 5 分鐘以上，記憶體中的 `LD_LIBRARY_PATH` 可能跟最新 build 不同步。`ros2 run` 出現「找不到自訂訊息類型」時：
```bash
source ~/ros2_ws/install/setup.bash    # 強制重新 source
```
特別是改完 .msg/.srv/.action **重 build 之後**必須重 source。

### 雷 8（TheConstruct 環境特有）：免費帳號 session 中斷
TheConstruct 免費帳號每次連線約 1 小時。session 中斷後 `ros2_ws/install/` 可能保留、可能不保留——再進 ROSject 時先 `colcon build` 一次保險。

---

## 🎯 學到的關鍵概念

- **三種 .* 檔案**：`.msg` (Topic) / `.srv` (Service) / `.action` (Action)
- **interface 套件分離**：純定義不含 code，是業界慣例
- **rosidl 生成器**：從文字檔自動生成 C++ class、Python class、CDR 序列化邏輯
- **巢狀型別**：用其他套件的型別（如 `std_msgs/Header`）必須宣告依賴
- **常數慣例**：`uint8 NAME=value` 自動生成 static const，避免 magic number
- **生成檔名規則**：PascalCase msg → snake_case 檔名 → snake_case .hpp
- **下游依賴**：在邏輯套件的 CMake + package.xml 都要 declare interface 套件

---

## 🌟 進階挑戰

1. **加 array 欄位**：在 BrakeStatus 加 `float32[] recent_distances`（過去 10 筆距離）
2. **巢狀自訂 type**：寫一個 `BrakeEvent.msg` 含 `BrakeStatus`，看怎麼 import 自己的型別
3. **Python client 訂閱自訂 msg**：寫個 rclpy 的 subscriber 訂 `/brake_status`，驗證跨語言通用
4. **Action 加超時 cancel**：modify approach_client，跑 5 秒後送 cancel，觀察 server 怎麼回 CANCELED

---

## 下一步

- [Phase 09 — Executors / Lifecycle / Composition](../phase-09-executors-lifecycle-composition/)（待完成）：學會 callback 怎麼被排程、Node 生命週期管理、多 Node 同 process

---

## 📁 完整檔案結構

```
phase-08-custom-interfaces/
├── README.md
├── code/
│   ├── my_robot_interfaces/                ← Interface 套件（純定義）
│   │   ├── package.xml
│   │   ├── CMakeLists.txt
│   │   ├── msg/BrakeStatus.msg
│   │   ├── srv/SetBrakeMode.srv
│   │   └── action/Approach.action
│   └── my_cpp_pkg/                         ← 邏輯套件（用 interfaces）
│       ├── package.xml
│       ├── CMakeLists.txt
│       └── src/
│           ├── smart_brake_v2.cpp          ← Pub + Sub + Service + Action 全在一個 Node
│           └── approach_client.cpp         ← Action client 範例
└── images/                                 ← (待補)
```
