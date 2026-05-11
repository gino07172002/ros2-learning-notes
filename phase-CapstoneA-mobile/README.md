# Capstone A:Mobile Robot 完整自主導航 demo

> 把 Phase 17 Gazebo + Phase 22A Nav2 + Phase 23A 自訂 BT plugin **整合成一條 launch**,加一個 C++ `auto_navigator` 自動依序送三個 waypoint 給 Nav2 action,**證明能寫出實機部署等級的整合 launch**。

**展示目標**:這是 Track A(Mobile)的最終整合作品,**履歷上一句話**:
> 「用 Nav2 + slam_toolbox 整合 TurtleBot3 在 Gazebo 內自動依序巡航三個 waypoint,自訂 BT plugin 做電量低檢查,完整 launch 一鍵啟動,GitHub 可重現。」

**整合的 phase**:
- [Phase 17 Gazebo](../phase-17-gazebo/) — simulator + turtlebot3
- [Phase 21A SLAM](../phase-21A-slam-toolbox/) — slam_toolbox 既有方法(本 capstone 用靜態 map,SLAM 留 README 討論)
- [Phase 22A Nav2](../phase-22A-nav2-basics/) — Nav2 8 個 lifecycle node
- [Phase 23A Nav2 BT plugin](../phase-23A-nav2-bt-plugin/) — 自訂 IsBatteryLow condition
- **新加:**`auto_navigator` C++ Node + capstone_a launch

**產出**:
- [`src/auto_navigator.cpp`](code/capstone_a/src/auto_navigator.cpp) — Action client + initialpose pub + 三個 waypoint
- [`launch/capstone_a.launch.py`](code/capstone_a/launch/capstone_a.launch.py) — 整合 launch

**環境**:☁️💻 雲端為主(WSL 結構驗證過,實際導航需 GPU)

---

## 🤔 為什麼這個 Capstone 強

對應**履歷上的 「Mobile Robot Autonomous Navigation Stack」 一行字**,具體支撐:

1. **多 package 整合** — capstone_a 依賴 my_gazebo_demo / my_nav2_demo / my_bt_plugin,整合測試「dependencies 真的接得起來」
2. **C++ Action Client** — 不只用 CLI 下 goal,寫 production-style 的 client 帶 feedback / result callback
3. **自動化** — 啟動後 `ros2 launch` 一條,自己跑完 initialpose + 三個 waypoint
4. **可擴展** — 把 waypoints_ vector 換成 yaml 讀,就是泛用 patrol robot
5. **GitHub-ready** — 全部用 launch + ament_cmake,clone repo + colcon build + ros2 launch 一鍵跑

---

## 🏗️ 完整架構

```
                          ┌───────────────────────┐
                          │ capstone_a.launch.py  │
                          └────────────┬──────────┘
                                       │ IncludeLaunchDescription
                                       ▼
                          ┌───────────────────────┐
                          │ my_nav2_demo (Phase 22A)│
                          └────────────┬──────────┘
                                       │ Include + Timer(10s)
                                       ▼
                ┌──────────────────────┴──────────────────────┐
                │                                             │
        ┌───────▼──────────┐                  ┌───────────────▼──────────┐
        │ my_gazebo_demo    │                  │ nav2_bringup             │
        │ (Phase 17)        │                  │ - map_server (static map)│
        │ - gzserver        │                  │ - amcl                   │
        │ - turtlebot3 SDF  │                  │ - controller_server (DWB)│
        │ - robot_state_pub │                  │ - planner_server (Navfn) │
        └───────────────────┘                  │ - smoother / behavior    │
                                               │ - bt_navigator           │
                                               │   └── plugin_lib_names:  │
                                               │       my_bt_plugin       │
                                               │       (Phase 23A)        │
                                               └──────────────────────────┘
                                       │ Timer(30s)
                                       ▼
                          ┌──────────────────────────────┐
                          │ auto_navigator(本章)          │
                          │ ─ pub /initialpose           │
                          │ ─ action client              │
                          │   /navigate_to_pose          │
                          │   sequential 3 waypoints     │
                          └──────────────────────────────┘
```

啟動 timeline:
- t=0:Gazebo + tb3 起來
- t=10s:Nav2 stack 啟動
- t=15–25s:Nav2 各 lifecycle Configure → Activate
- t=30s:auto_navigator 啟動
- t=35s:auto_navigator 發 `/initialpose`
- t=55s:發 goal 0 = (1.0, 0.0)
- t=…:依序跑完 (1, 0) → (1, 1) → (0, 0)

---

## 💻 重點檔案

### 1. src/auto_navigator.cpp — Action client 跑 waypoint sequence

完整見 [`src/auto_navigator.cpp`](code/capstone_a/src/auto_navigator.cpp)。亮點:

```cpp
// 階段一:啟動 5s 後送 initial pose
initialpose_timer_ = create_wall_timer(5s, [this]() { sendInitialPose(); });

// 階段二:啟動 25s 後開始送 goal(等 nav2 fully active)
start_timer_ = create_wall_timer(25s, [this]() { sendNextGoal(); });

void sendNextGoal()
{
  if (goal_index_ >= waypoints_.size()) return;

  auto [x, y] = waypoints_[goal_index_];
  NavigateToPose::Goal goal;
  goal.pose.header.frame_id = "map";
  goal.pose.pose.position.x = x;
  goal.pose.pose.position.y = y;
  goal.pose.pose.orientation.w = 1.0;

  rclcpp_action::Client<NavigateToPose>::SendGoalOptions opts;
  opts.feedback_callback = [this](auto, auto fb) {
    RCLCPP_INFO(get_logger(),
      "[Capstone A] goal %zu | distance_remaining=%.2f m",
      goal_index_, fb->distance_remaining);
  };
  opts.result_callback = [this](const auto & wrapped) {
    if (wrapped.code == rclcpp_action::ResultCode::SUCCEEDED) {
      RCLCPP_INFO(get_logger(), "[Capstone A] ✅ goal %zu reached", goal_index_);
    }
    goal_index_++;
    next_goal_timer_ = create_wall_timer(5s, [this]() { sendNextGoal(); });
  };

  nav_client_->async_send_goal(goal, opts);
}
```

**設計亮點**:
- 用 lambda + capture 寫 callback,精簡
- result_callback 內排下一個 goal,完全 event-driven
- 三個 waypoint 寫死在 vector,改 yaml 讀就是泛用 patrol
- `wait_for_action_server(5s)` 防禦性編程,Nav2 沒起來就 log error 不崩

### 2. launch/capstone_a.launch.py

```python
# 一條 launch 帶起整個 stack
nav2 = IncludeLaunchDescription(...nav2_demo.launch.py)   # Phase 22A
auto_nav = TimerAction(period=30.0, actions=[
    Node(package='capstone_a', executable='auto_navigator')])

return LaunchDescription([nav2, auto_nav])
```

**短而強** — 只有 5 行有效程式,因為前面 phase 都已經把細節打包好了。**可重用設計的勝利**。

---

## 🚀 完整 Demo 流程

### ☁️ TheConstructSim 步驟(推薦 — Capstone 完整跑得起來)

整套 Capstone A 的所有依賴(Gazebo + Nav2 + slam_toolbox + turtlebot3)雲端 ROSject 都預載,**比 WSL 順很多**。

```bash
# 1. 雲端 ROSject terminal,clone 整個 repo(一次帶齊所有 phase)
cd ~/ros2_ws/src
git clone https://github.com/gino07172002/ros2-learning-notes.git
cp -r ros2-learning-notes/phase-17-gazebo/code/my_gazebo_demo .
cp -r ros2-learning-notes/phase-22A-nav2-basics/code/my_nav2_demo .
cp -r ros2-learning-notes/phase-23A-nav2-bt-plugin/code/my_bt_plugin .
cp -r ros2-learning-notes/phase-CapstoneA-mobile/code/capstone_a .

# 2. Build 全部
export TURTLEBOT3_MODEL=burger
cd ~/ros2_ws
colcon build --packages-select my_gazebo_demo my_nav2_demo my_bt_plugin capstone_a
source install/setup.bash

# 3. 跑 Capstone A
ros2 launch capstone_a capstone_a.launch.py

# 4. 看 Tools → Gazebo + Tools → Graphical Tools (RViz)
#    車會自動依序走 3 個 waypoint
```

**雲端預期**:車真的會在 Gazebo 內跑 → 經過 waypoint → 完成 sequence。
**WSL 預期**:lifecycle active 但車卡住(GPU 不足,雷 4)。

---

### 💻 WSL 步驟(WSL 驗證過 — 結構驗證,實際導航需 GPU)

#### Step 1:確認所有依賴 phase 已 build

```bash
# 必須這些都在 ~/ros2_ws/install
ls ~/ros2_ws/install/my_gazebo_demo
ls ~/ros2_ws/install/my_nav2_demo
ls ~/ros2_ws/install/my_bt_plugin
```

如果哪個沒 build,回去 Phase 17 / 22A / 23A 各自跑 `colcon build`。

### Step 2:部署 + build Capstone A

```bash
rm -rf ~/ros2_ws/src/capstone_a
cp -r /mnt/d/ros_learn/ros2-learning-notes/phase-CapstoneA-mobile/code/capstone_a \
      ~/ros2_ws/src/capstone_a
source /opt/ros/humble/setup.bash
cd ~/ros2_ws && colcon build --packages-select capstone_a
```

### Step 3:啟動

```bash
export TURTLEBOT3_MODEL=burger
ros2 launch capstone_a capstone_a.launch.py
```

### Step 4:驗證(實測 log)

```
[component_container_isolated-4] [lifecycle_manager_localization]: Starting managed nodes bringup...
[component_container_isolated-4] [lifecycle_manager_localization]: Managed nodes are active   ← ✅
[component_container_isolated-4] [lifecycle_manager_navigation]: Starting managed nodes bringup...
[INFO] [auto_navigator-5]: process started with pid [14183]
[auto_navigator-5] [INFO] [auto_navigator]: [Capstone A] sent /initialpose (0,0)
[component_container_isolated-4] [INFO] [amcl]: initialPoseReceived       ← ✅ amcl 收到 initial pose
[auto_navigator-5] [INFO] [auto_navigator]: [Capstone A] sending goal 0: (1.0, 0.0)   ← ✅ 第一個 goal 送出
```

**WSL 結構驗證過的事情**:
- ✅ 所有 dependencies 整合 build 過
- ✅ 整條 launch 能起,沒任何「找不到 package」之類錯
- ✅ Nav2 兩組 lifecycle 都 active
- ✅ auto_navigator 啟動 + 自動發 initialpose + amcl 收到
- ✅ goal 送進 /navigate_to_pose action

**WSL 結構過但效能不過的事情**(GPU 不足,Phase 21A/22A 已詳述):
- amcl message filter dropping → localization 不準
- controller / planner 沒拿到完整 sensor stack → 車不動或亂晃

**雲端 / 實機(GPU 充足)預期**:車自己依序跑完三個 waypoint,RViz 看到綠色 plan 路徑跟車跡。

---

## 🐛 常見雷

### ⚠️ 雷 1:Nav2 還沒 active 就送 goal,client 卡 wait_for_action_server

**症狀**:
```
[Capstone A] /navigate_to_pose action server not available — Nav2 起來了嗎?
```
auto_navigator 等不到 nav2 的 action server,直接 abort。

**原因**:Nav2 lifecycle activate 要 5–10 秒(WSL 慢還更久)。auto_navigator delay 不夠。

**解**:capstone_a.launch.py 的 `TimerAction(period=30.0)` 等 30 秒。WSL 慢的話拉到 45 秒。

### ⚠️ 雷 2:depends 沒寫 my_bt_plugin,plugin 沒 build,Nav2 啟動時報「missing plugin」

**症狀**:Nav2 啟動 log 出現:
```
[bt_navigator]: Could not load library libis_battery_low_condition_bt_node.so
```

**原因**:capstone_a 雖然 launch include nav2,但 nav2 params 內如果加了 `plugin_lib_names: ["is_battery_low_condition_bt_node"]`,bt_navigator 起來找這個 .so,**.so 由 my_bt_plugin package install** — package.xml 沒寫 `<exec_depend>my_bt_plugin</exec_depend>`,colcon 不會自動把它放到 path。

**解**:capstone_a/package.xml 內必加:
```xml
<exec_depend>my_bt_plugin</exec_depend>
```

### ⚠️ 雷 3:double include Gazebo,gzserver 起兩次 → port 11345 衝突

**症狀**:
```
gzserver: bind: Address already in use
```

**原因**:capstone_a launch 同時 include `my_gazebo_demo` 跟 `my_nav2_demo`,而 my_nav2_demo 自己也 include my_gazebo_demo。gz 起兩次,port 衝。

**解**:capstone_a 只 include `my_nav2_demo`(它已經帶 Gazebo)。**不要再單獨 include Gazebo**。本章 launch 已修正。

### ⚠️ 雷 4:`use_sim_time` 在 capstone Node 內沒設,跟 Nav2 對不上

**症狀**:auto_navigator 發 goal,timestamp 是 wall time,Nav2 收到後 transform 對不上,goal 立刻 abort。

**原因**:Nav2 用 sim time(由 launch 傳 `use_sim_time:=true`),但 capstone 的 Node 預設 wall time。

**解**:auto_navigator 啟動時加 `parameters=[{'use_sim_time': True}]`。本章 launch 已加。

### ⚠️ 雷 5:waypoints 太靠近障礙物,Nav2 規劃失敗

**症狀**:result_callback 收到 `code = ABORTED`,goal 沒到。

**原因**:你設的 waypoint 距離牆 < inflation_radius(預設 0.55m),Nav2 認為「目標在障礙裡」,planner 直接拒絕。

**解**:
- 4×4m world 的可走範圍是 (-1.5, 1.5) 大概(扣 inflation),別放在 (1.9, 0)
- 或調 `inflation_layer.inflation_radius: 0.3` 讓機器人能更靠牆

---

## 🎯 學到的關鍵概念

| 概念 | 一句話 |
|------|------|
| Multi-package launch include | 大型 robotics 系統靠 launch include 串接 phase |
| `IncludeLaunchDescription` | launch 內呼叫另一個 launch,可傳 args |
| `TimerAction` | 拖延 launch 順序避免 race |
| Action client lambda callbacks | feedback/result 用 lambda + capture,精簡 |
| Sequential waypoint navigation | result_callback 內 schedule 下一個 goal |
| `<exec_depend>` 在 capstone | runtime 依賴(plugin .so 等),不寫 colcon 不接 |
| double include Gazebo 雷 | port 11345 只能一個,launch 樹要小心 |

---

## 🌟 進階挑戰

1. **YAML 讀 waypoints**:把寫死的 vector 換成讀 `waypoints.yaml`,做泛用 patrol robot
2. **加 SLAM 模式**:不用 static map,改 launch 包 `slam_toolbox` 一邊建圖一邊導航(Nav2 的 `slam_launch.py` 直接套)
3. **加 RViz panel**:Capstone 啟動同時開 RViz,使用者可看到 amcl 粒子、costmap、plan
4. **錄影**:用 `ros2 bag record /tf /scan /odom /cmd_vel /amcl_pose /plan` 錄完整 demo,GitHub 放 link
5. **Docker 化(Capstone Final 預告)**:把這整套包成 docker compose,clone repo + `docker compose up`

---

## 🔗 下一步

- **Capstone Final** — 把這個 Capstone 用 Docker 化,push 到 GHCR,做完整生產化展示
- **Phase 27 部署實機**(待完成) — 把這個 launch 部署到 Pi/Jetson + 實體 turtlebot3,看現實世界跑
- **回頭** — Track B(MoveIt)為機械手臂做類似的 Capstone B

---

## 📁 完整檔案結構

```
phase-CapstoneA-mobile/
├── README.md
├── code/
│   └── capstone_a/
│       ├── package.xml
│       ├── CMakeLists.txt
│       ├── src/
│       │   └── auto_navigator.cpp        ← Action client + initialpose
│       └── launch/
│           └── capstone_a.launch.py      ← 整合所有 phase 的 launch
└── images/                              ← (之後補:RViz 三個 waypoint 跑完截圖)
```
