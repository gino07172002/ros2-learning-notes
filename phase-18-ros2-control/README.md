# Phase 18：ros2_control — 硬體抽象層

> 業界 AGV、協作手臂、所有實體機器人的標準框架。學會它你能讓「同一份控制程式」跑在 Gazebo 模擬、跑在實機，零改動。

**學完你會**：URDF 內加 `<ros2_control>` 區塊、寫 controllers.yaml、用 controller_manager 載入 controller、送速度命令觀察 state 變化、了解硬體抽象層為什麼業界必備。

**前置**：[Phase 15 URDF](../phase-15-urdf/) + [Phase 19 pluginlib](../phase-19-pluginlib/)（ros2_control 內部就是 pluginlib）

**產出**：`my_robot_bringup` 套件含 URDF + controllers.yaml + 整套 launch

**環境**:☁️💻 雙環境通用 — 本章用 `mock_components`(假硬體),**不需要 Gazebo / GPU / 實機**,所以雲端 TheConstructSim 也能完整跑。

> ☁️ **想用 TheConstruct 雲端跑**:任意 ROS 2 Humble ROSject 都可以。Clone repo → `cp -r phase-18-ros2-control/code/my_robot_bringup ~/ros2_ws/src/` → `colcon build` → `ros2 launch my_robot_bringup demo.launch.py`,完全相同步驟。`mock_components::GenericSystem` 不需要硬體,純 ROS 控制流就跑得起來。

---

## 為什麼 ros2_control 業界必備

業界寫機器人控制最大痛點：**模擬與實機不一樣**。
- 在 Gazebo 模擬時，「送速度給輪子」是 publish 到 Gazebo plugin
- 在實機，「送速度給輪子」是寫到馬達 driver 的 CAN bus / serial
- 公司常見作法：寫兩份 code → bug 加倍、人力浪費

ros2_control 的答案：**統一介面**。
```
你的主程式  ──────▶ controller (plugin)
                          │
                          ▼ command_interface
                   ┌──────────────┐
                   │ HardwareInterface (plugin) │
                   ├──────────────┤
                   │ Gazebo / Mock / Real Robot │
                   └──────────────┘
                          │
                          ▼ state_interface
你的主程式  ◀──────  controller (plugin)
```

**主程式只關心 controller、不知道下面是 Gazebo 還是真機**。

業界職位：「會 ros2_control 的工程師」薪資普遍比一般 ROS 開發者高 20–30%。

---

## 🏗️ ros2_control 三層架構

### Layer 1：Hardware Interface（最底層）

「假裝」自己是真硬體。在 URDF 內聲明：

```xml
<ros2_control name="GenericSystem" type="system">
  <hardware>
    <plugin>mock_components/GenericSystem</plugin>
    <!-- 真實情境會換成：
         <plugin>my_robot_hw/MyMotorDriver</plugin> -->
  </hardware>

  <joint name="left_wheel_joint">
    <command_interface name="velocity">       <!-- 主程式可以寫的 -->
      <param name="min">-10</param>
      <param name="max">10</param>
    </command_interface>
    <state_interface name="position" />        <!-- 主程式可以讀的 -->
    <state_interface name="velocity" />
  </joint>
</ros2_control>
```

**`mock_components/GenericSystem`** 是學習用的「假硬體」——收到 command 就直接當 state 回報，不需要 Gazebo 也不需要真機。

業界常見實作：
- `gazebo_ros2_control` — 接 Gazebo 模擬
- `your_robot_hw` — 自己寫的 plugin 連 CAN bus / serial

### Layer 2：Controller（中層）

`controller_manager` 載入 controller plugins，每個 controller 訂閱命令、做計算、寫到 hardware command_interface。

`controllers.yaml`：
```yaml
controller_manager:
  ros__parameters:
    update_rate: 100  # Hz

    joint_state_broadcaster:                # ← 把 state 發到 /joint_states
      type: joint_state_broadcaster/JointStateBroadcaster

    velocity_controller:                    # ← 接收速度命令、寫到硬體
      type: velocity_controllers/JointGroupVelocityController

velocity_controller:
  ros__parameters:
    joints: [left_wheel_joint, right_wheel_joint]
    interface_name: velocity
```

ROS 2 內建一堆 controller：
| Controller | 用途 |
|-----------|------|
| `joint_state_broadcaster` | 必載——發送 /joint_states |
| `velocity_controllers/JointGroupVelocityController` | 多關節速度命令 |
| `position_controllers/JointGroupPositionController` | 多關節位置命令 |
| `joint_trajectory_controller/JointTrajectoryController` | 軌跡（MoveIt 用這個）|
| `diff_drive_controller/DiffDriveController` | 差速車的 cmd_vel → 兩輪速度 |

**自己寫 controller**就是 pluginlib（Phase 19）的進階應用。

### Layer 3：使用者程式（最上層）

對你的主程式來說，要做的事極簡：
```bash
# 訂閱 /joint_states 看狀態
# 發布 /<controller_name>/commands 送命令
ros2 topic pub /velocity_controller/commands std_msgs/msg/Float64MultiArray '{data: [2.0, 2.0]}'
```

---

## 🚀 Demo 流程

### Step 1：部署 + build

```bash
cp -r /mnt/d/ros_learn/ros2-learning-notes/phase-18-ros2-control/code/my_robot_bringup \
      ~/ros2_ws/src/

cd ~/ros2_ws
colcon build --packages-select my_robot_bringup
source install/setup.bash
```

### Step 2：啟動 ros2_control 系統

```bash
ros2 launch my_robot_bringup control.launch.py
```

預期 log（**驗證過**）：
```
Loading hardware 'GenericSystem'
Successful initialization of hardware 'GenericSystem'
Successful 'configure' of hardware 'GenericSystem'
Successful 'activate' of hardware 'GenericSystem'
update rate is 100 Hz
Loading controller 'velocity_controller'
Loading controller 'joint_state_broadcaster'
Configuring controller 'velocity_controller' → configure successful
Configured and activated velocity_controller
Configured and activated joint_state_broadcaster
```

🎯 **這個 log 流程要記住**——業界看 ros2_control bringup 都是這樣。

### Step 3：列出系統狀態

```bash
ros2 control list_controllers
# velocity_controller     velocity_controllers/JointGroupVelocityController  active
# joint_state_broadcaster joint_state_broadcaster/JointStateBroadcaster      active

ros2 control list_hardware_interfaces
# command interfaces
#   left_wheel_joint/velocity  [available] [claimed]
#   right_wheel_joint/velocity [available] [claimed]
# state interfaces
#   left_wheel_joint/position + velocity
#   right_wheel_joint/position + velocity
```

🎯 `[claimed]` 表示這個 interface 被某個 active controller 佔用了。

### Step 4：送速度命令觀察狀態

新 terminal：
```bash
# 看當前 state
ros2 topic echo /joint_states --once
# velocity: [0.0, 0.0]

# 送速度命令（左輪 2 rad/s, 右輪 2 rad/s）
ros2 topic pub --once /velocity_controller/commands std_msgs/msg/Float64MultiArray \
  '{data: [2.0, 2.0]}'

# 再看 state — velocity 已經變
ros2 topic echo /joint_states --once
# velocity: [2.0, 2.0]    ← mock_components 把 command 當作 state 回報
```

🎯 **這就是 ros2_control 的全套**：command 送進去、state 出來。在 Gazebo / 真機都是同樣流程。

### Step 5：手動切換 controller（業界常用）

```bash
# 停掉 velocity_controller
ros2 control set_controller_state velocity_controller inactive

# 載入並啟動 position_controller（如果你有設）
# ros2 control switch_controllers --activate position_controller

# 看狀態
ros2 control list_controllers
# velocity_controller: inactive
```

業界場景：機械臂從「教導模式（trajectory_controller）」切到「自由拖曳模式（admittance_controller）」就靠這個。

---

## 🐛 常見雷

### 雷 1：URDF `<ros2_control>` 區塊忘了
controller_manager 會抱怨「找不到 hardware」。**這個 tag 必須跟 `<link>` `<joint>` 放同層**，在 `<robot>` 裡。

### 雷 2：command_interface 跟 controller type 對不上
```xml
<command_interface name="velocity" />   <!-- URDF 提供 velocity -->
```
```yaml
velocity_controller:
  type: velocity_controllers/JointGroupVelocityController   # ← 也要 velocity
```
URDF 寫 `velocity` 但 controller 是 position controller → 起不來。

### 雷 3：spawner 在 controller_manager 起來前跑
```python
# ❌ 直接放 LaunchDescription → 兩個並行啟動 → spawner 先跑會失敗
return LaunchDescription([controller_manager, spawner])

# ✅ 用 RegisterEventHandler 等 controller_manager 起來才跑 spawner
RegisterEventHandler(
    OnProcessStart(
        target_action=controller_manager,
        on_start=[spawner],
    )
)
```

### 雷 4：FIFO 排程 warning
```
Could not enable FIFO RT scheduling policy: Operation not permitted
```
**正常**——非 root 跑沒辦法用 RT scheduler。production 部署時會給 ros2_control_node `CAP_SYS_NICE` capability。本章學習階段忽略。

### 雷 5：robot_description 重複 parameter
Humble 警告：「passing robot_description directly is deprecated, use the topic」。**目前還能用**，未來版本會改。

### 雷 6：mock_components 找不到
```bash
ros2 pkg list | grep ros2_control
# 必須有 ros2_control + ros2_controllers
```
sudo apt install ros-humble-ros2-control ros-humble-ros2-controllers

---

## 🎯 學到的關鍵概念

- **三層架構**：Hardware Interface / Controller / 使用者
- **URDF `<ros2_control>` 區塊**：宣告 hardware + 每個 joint 的 interfaces
- **mock_components**：學習用假硬體，不需 Gazebo
- **controller_manager**：核心 Node，載入 controller plugins
- **`ros2 control` CLI**：list_controllers / list_hardware_interfaces / switch
- **command 送進、state 出來**：模擬與實機通用流程
- **業界職缺剛需**

---

## 🌟 進階挑戰

1. **改用 diff_drive_controller**：訂 /cmd_vel 而不是 commands，自動換算成兩輪速度
2. **加 trajectory controller**：用 `JointTrajectoryController` 接收軌跡命令（MoveIt 也是用這個）
3. **接 Gazebo**：把 `mock_components/GenericSystem` 換成 `gazebo_ros2_control/GazeboSystem`
4. **寫自己的 hardware plugin**：實作 `hardware_interface::SystemInterface`，連 fake serial port

---

## 下一步

- [Phase 17 — Gazebo 整合](../phase-17-gazebo/)
- [Phase 20 — 多機通訊](../phase-20-multi-machine/)

---

## 📁 完整檔案結構

```
phase-18-ros2-control/
├── README.md
└── code/
    └── my_robot_bringup/
        ├── package.xml
        ├── CMakeLists.txt
        ├── urdf/
        │   └── robot_with_control.urdf.xacro     ← URDF + ros2_control 區塊
        ├── config/
        │   └── controllers.yaml                  ← controller_manager 設定
        └── launch/
            └── control.launch.py                 ← 完整啟動含 spawner
```
