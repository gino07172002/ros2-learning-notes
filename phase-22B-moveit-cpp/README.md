# Phase 22B:MoveIt 2 C++ 程式控制

> 用 `MoveGroupInterface` C++ API 規劃 6-DOF 手臂(Phase 20B URDF)的軌跡。**4 種 plan target**:SRDF named pose、joint values、Cartesian pose、return home。**完全純 CLI 驗證**(不需要 RViz)。

**這章你將解鎖的業界 C++ 手臂控制技能**：
- **駕馭最高階的控制介面 (`MoveGroupInterface`)**：擺脫底層繁瑣的通訊協議，學會用 MoveIt 官方提供的 C++ 類別，只用寥寥數行程式碼就完成複雜的碰撞閃避與路徑規劃。
- **掌握三種必備的戰略目標**：學會使用預先命名的姿態 (`setNamedTarget`)、六軸關節的絕對數值 (`setJointValueTarget`) 以及末端點的三維空間座標 (`setPoseTarget`) 來下達導航指令，應對工廠中各種不同的夾取情境。
- **破解 ROS 2 參數傳遞的世紀大雷**：看懂為什麼你的 C++ 程式總是抓不到機器人的 URDF 模型。學會在 Launch 檔中優雅地將三大核心描述參數 (`description`, `semantic`, `kinematics`) 強制餵進獨立的 Node 中。
- **手刻最小可行性設定檔**：在依賴 GUI 自動產出之前，親手撰寫支撐 MoveIt 運作的四大支柱：運動學 (Kinematics)、路徑演算法 (OMPL)、關節極限 (Joint Limits) 與控制器 (Controllers) YAML 檔。
- **直面 IK (逆向運動學) 的無解崩潰**：深刻體會終端機噴出 `Unable to sample any valid states` 時的絕望，並學會如何透過放寬容忍度或除錯點位來解決空間中「手骨折也構不到」的座標點。

**前置**:
- [Phase 20B 手臂 URDF](../phase-20B-arm-urdf/) — 6-DOF arm URDF + SRDF
- [Phase 09 Lifecycle / Composition](../phase-09-executors-lifecycle-composition/) — `MultiThreadedExecutor` / spinning Node 觀念
- [Phase 06 Parameters](../phase-06-parameters/) — MoveIt 用大量 yaml 參數

**產出**:
- [`my_arm_moveit_config/`](code/my_arm_moveit_config/) — 完整 MoveIt config(yaml + launch)
- [`my_arm_moveit_demo/src/plan_demo.cpp`](code/my_arm_moveit_demo/src/plan_demo.cpp) — 4 個 plan demo

**環境**:☁️💻 雙環境通用(純文字驗證,不需 GUI)

---

## 🤔 為什麼這章重要

業界機械手臂 100% 用 MoveIt 規劃。Phase 20B 寫了 URDF,**但沒 MoveIt 連動 = 是個能畫但不能動的機械手臂**。

`MoveGroupInterface` 是 MoveIt 給「應用層程式」的高階 API:**寫一行 setNamedTarget("home") + plan() = 規劃完成**,中間 IK / collision check / OMPL 全幫你做掉。

這章故意**不啟動 RViz** 而用文字 log 驗證,證明:
1. MoveIt 的 plan 結果(軌跡點數、時間)可程式取得
2. portfolio 級的 demo **不依賴 GUI**,CI 也可跑(實機部署常無 GUI)
3. RViz 視覺驗證留給雲端 / 你本機之後補

> 💡 **想用 GUI 自動產這 4 個 yaml + SRDF + self-collision matrix**?看 [Phase 21B MoveIt Setup Assistant](../phase-21B-moveit-setup-assistant/) — 業界做法是用 wizard 一鍵生成,本章手寫是教學用,讓你看清楚「最少需要什麼」。**真實專案先 21B 再 22B 微調**。

---

## 🏗️ 架構

```
┌──────────────────────────────────────────────────────────────┐
│ my_arm_moveit_config/launch/move_group.launch.py              │
│                                                                │
│  ┌─ robot_state_publisher ──┐                                  │
│  │  reads URDF (Phase 20B)  │                                  │
│  └──────────────────────────┘                                  │
│  ┌─ joint_state_publisher ──┐                                  │
│  │  發 /joint_states        │                                  │
│  └──────────────────────────┘                                  │
│  ┌─ move_group ─────────────┐                                  │
│  │  parameters:             │                                  │
│  │    robot_description     │ ← URDF                           │
│  │    robot_description_    │                                  │
│  │      semantic            │ ← SRDF (Phase 20B)               │
│  │    robot_description_    │                                  │
│  │      kinematics          │ ← kinematics.yaml (KDL plugin)   │
│  │    planning_pipelines    │ ← ompl_planning.yaml             │
│  │    moveit_simple_        │                                  │
│  │      controller_manager  │ ← moveit_controllers.yaml        │
│  └──────────────────────────┘                                  │
└──────────────────────────────────────────────────────────────┘
                            ▲
                            │ /move_action (action client)
                            │
┌──────────────────────────────────────────────────────────────┐
│ my_arm_moveit_demo/src/plan_demo.cpp                          │
│                                                                │
│  MoveGroupInterface arm(node, "arm");                         │
│  arm.setNamedTarget("ready");                                 │
│  arm.plan(plan);                                              │
│  // ↑ 透過 /move_action 跟 move_group 互動                    │
└──────────────────────────────────────────────────────────────┘
```

---

## 💻 重點檔案

### 1. moveit config 4 個 yaml

完整見 [`my_arm_moveit_config/config/`](code/my_arm_moveit_config/config/)。

**kinematics.yaml** — IK 用什麼 plugin:
```yaml
arm:
  kinematics_solver: kdl_kinematics_plugin/KDLKinematicsPlugin
  kinematics_solver_search_resolution: 0.005
  kinematics_solver_timeout: 0.05
```

**ompl_planning.yaml** — Path planning algorithm:
```yaml
planning_plugin: ompl_interface/OMPLPlanner
arm:
  default_planner_config: RRTConnectkConfigDefault
  planner_configs:
    RRTConnectkConfigDefault:
      type: geometric::RRTConnect
```

**joint_limits.yaml** — 速度 / 加速度上限(URDF 內已有 position 限制,這裡補 v/a)
**moveit_controllers.yaml** — 假 trajectory controller(實機要換成 ros2_control)

### 2. plan_demo.cpp — MoveGroupInterface 的核心 API

完整見 [`my_arm_moveit_demo/src/plan_demo.cpp`](code/my_arm_moveit_demo/src/plan_demo.cpp)。

關鍵流程:

```cpp
auto node = std::make_shared<rclcpp::Node>("plan_demo",
  rclcpp::NodeOptions().automatically_declare_parameters_from_overrides(true));

// 用獨立 thread spin executor — MoveGroupInterface 內部會呼 service / action
rclcpp::executors::SingleThreadedExecutor executor;
executor.add_node(node);
std::thread spinner([&executor]() { executor.spin(); });

MoveGroupInterface arm(node, "arm");      // group 名來自 SRDF
arm.setPlanningTime(5.0);

// ── 4 種 target,每個 plan 完都拿到 trajectory ──

// 1. SRDF named pose
arm.setNamedTarget("ready");
MoveGroupInterface::Plan plan;
arm.plan(plan);                            // 回 MoveItErrorCode

// 2. joint values
arm.setJointValueTarget({0.5, -0.3, 0.6, 0.0, 0.8, 0.0});
arm.plan(plan);

// 3. cartesian pose(會跑 IK)
geometry_msgs::msg::Pose target;
target.position = {0.2, 0.0, 0.9};
target.orientation.w = 1.0;
arm.setPoseTarget(target);
arm.plan(plan);

// 4. 回 home
arm.setNamedTarget("home");
arm.plan(plan);
```

每個 `plan` 完讀 `plan.trajectory_.joint_trajectory.points.size()` 看軌跡點數,
讀 `plan.planning_time_` 看 OMPL 解花多久。

### 3. plan_demo.launch.py — 雙 launch include + parameter sharing

關鍵:**plan_demo Node 也要拿到 robot_description / semantic / kinematics**(不只 move_group)。
詳見雷 1。

```python
# move_group 從 my_arm_moveit_config 來
move_group = IncludeLaunchDescription(...move_group.launch.py)

# plan_demo 額外帶上同樣的 3 份 description params
plan_demo = TimerAction(period=8.0, actions=[
    Node(package='my_arm_moveit_demo', executable='plan_demo',
         parameters=[robot_description, robot_description_semantic, kinematics])])
```

---

## 🚀 完整 Demo 流程(WSL,驗證過)

### Step 1:確認 my_arm_description 已 build(從 Phase 20B)

```bash
ls ~/ros2_ws/install/my_arm_description
```

沒的話回 Phase 20B 跑一次 colcon build。

### Step 2:部署 + build

```bash
rm -rf ~/ros2_ws/src/my_arm_moveit_config ~/ros2_ws/src/my_arm_moveit_demo
cp -r /mnt/d/ros_learn/ros2-learning-notes/phase-22B-moveit-cpp/code/my_arm_moveit_config \
      ~/ros2_ws/src/my_arm_moveit_config
cp -r /mnt/d/ros_learn/ros2-learning-notes/phase-22B-moveit-cpp/code/my_arm_moveit_demo \
      ~/ros2_ws/src/my_arm_moveit_demo
source /opt/ros/humble/setup.bash
cd ~/ros2_ws && colcon build --packages-select my_arm_moveit_config my_arm_moveit_demo
```

驗證過輸出:
```
Starting >>> my_arm_moveit_config
Finished <<< my_arm_moveit_config [7.87s]
Starting >>> my_arm_moveit_demo
Finished <<< my_arm_moveit_demo [46.2s]
```

`moveit_demo` build 慢(~46s)是因為 link `moveit_ros_planning_interface` 把整個 MoveIt 拉進來。

### Step 3:啟動 + 4 個 plan demo

```bash
ros2 launch my_arm_moveit_demo plan_demo.launch.py
```

**驗證過實測 log**:

```
[plan_demo] connected to group 'arm'
  planning_frame=world
  end_effector_link=tool0
  joint_count=6

[named pose 'ready']           ✅ plan OK | points= 73 | duration=7.167s | planning_time=0.057s
[joint values target]          ✅ plan OK | points= 60 | duration=5.834s | planning_time=0.015s
[cartesian pose (0.2,0,0.9)]   ✅ plan OK | points=102 | duration=10.016s| planning_time=0.024s
[named pose 'home' (return)]   ✅ plan OK | points=  1 | duration=0.000s | planning_time=0.001s
[plan_demo] all demos done
```

**4 個 demo 全部 ✅**。OMPL RRTConnect 解時間 < 0.1s,軌跡 1–102 點。

注意:
- "home (return)" points=1 是因為車**已經在 home 位置**(預設 joint=0)→ 軌跡只有 0 點。MoveIt 偵測到「已在目標」直接回 1 點假軌跡
- cartesian pose 一開始在 (0.3, 0.2, 0.6) 規劃失敗(`Unable to sample any valid states`),改成 (0.2, 0, 0.9) 後 OK。詳見雷 5

### Step 4:RViz 視覺驗證(留給雲端)

```bash
ros2 launch my_arm_moveit_demo plan_demo.launch.py &
ros2 run rviz2 rviz2
# Add → MotionPlanning → Robot Description: robot_description
# 看 plan_demo 跑 4 個 target 時,RViz 內手臂跟著動
```

---

## 🐛 常見雷

### ⚠️ 雷 1:**plan_demo Node 噴 `Unable to parse SRDF` / `Could not find parameter robot_description_semantic`**

**症狀(實測)**:
```
[ERROR] [plan_demo]: Could not find parameter robot_description_semantic
                     and did not receive ... within 10.000000 seconds.
[FATAL] [move_group_interface]: Unable to construct robot model.
```
plan_demo 立刻 abort。

**原因**:`MoveGroupInterface` 構造時要從 ROS parameter 讀 `robot_description` / `robot_description_semantic` / `robot_description_kinematics`。**這些 param 設在 move_group 上,不會自動共享給 plan_demo**(每個 Node 有自己的 parameter namespace)。

**解**:plan_demo Node 啟動時也明確帶上同樣 3 份 params:
```python
Node(package='my_arm_moveit_demo', executable='plan_demo',
     parameters=[robot_description, robot_description_semantic, kinematics])
```

**這是 MoveIt 2 ROS 2 移植版最常踩的雷**,ROS 1 時代用 global param 不會碰到。

### ⚠️ 雷 2:`automatically_declare_parameters_from_overrides=true` 沒設,參數讀不到

**症狀**:plan_demo 拿到 robot_description 但 MoveGroupInterface 內部 `robot_description_kinematics.arm.kinematics_solver` 讀不到。

**原因**:ROS 2 預設嚴格 declare:沒 declare 的 param 讀不到,即使 launch 餵進來。MoveIt 內部沒 declare 嵌套 yaml 的 sub-key,要 NodeOptions 開「自動 declare from overrides」。

**解**:
```cpp
auto node = std::make_shared<rclcpp::Node>("plan_demo",
  rclcpp::NodeOptions().automatically_declare_parameters_from_overrides(true));
```

不寫這個 → MoveIt 內部讀 `arm.kinematics_solver` 拿到 default `chain_chain_solver`,IK 全部失敗。

### ⚠️ 雷 3:**`Unable to sample any valid states for goal tree`** — IK 解不出來

**症狀(實測)**:cartesian pose target 噴:
```
[ompl]: arm/arm: Unable to sample any valid states for goal tree
[ParallelPlan::solve()]: Unable to find solution by any of the threads in 5.0 seconds
```
plan FAILED,planning_time 用滿 5 秒。

**原因**:你給的 cartesian pose 在 IK **無解**或在 workspace 邊緣。`(0.3, 0.2, 0.6)` 對這支 6-DOF 手臂超出可達範圍 — 這支臂直立時 tool0 在 `(0, 0, 1.10)`,workspace 是個半徑 ~1m 的球。

**解**:
1. 先用 `arm.getCurrentPose()` 看當前 pose,從那 offset 出發
2. 用 `setRandomTarget()` 試試 — 它會生 reachable 的 pose,你可印出來看可達範圍長啥樣
3. 設更鬆的 `setGoalPositionTolerance(0.05)` / `setGoalOrientationTolerance(0.1)` 給 IK 更大解空間
4. 確保 SRDF 內 named pose 寫在 reachable 範圍

### ⚠️ 雷 4:`No 3D sensor plugin(s) defined for octomap updates`

**症狀**:move_group 啟動時噴 ERROR(看似嚴重)。

**原因**:MoveIt 預設啟用 `OccupancyMapMonitor`,等深度感測器(Kinect / RealSense)的 PointCloud2 訊息更新 octomap(碰撞 grid)。我們沒接深度相機 → 沒 plugin 設定 → ERROR log。

**解**:**忽略**。不影響規劃功能。要乾淨可在 yaml 加:
```yaml
sensors: []
```

或用 `octomap_monitor.enabled: False`。Phase 22B 沒做這事,實機接深度相機才需設。

### ⚠️ 雷 5:`Could not identify parent group for end-effector 'tool'`

**症狀(實測)**:plan_demo 啟動 WARN:
```
moveit_robot_model.robot_model: Could not identify parent group for end-effector 'tool'
```

**原因**:Phase 20B 的 SRDF 有 `<end_effector name="tool" parent_link="tool0" group="arm"/>`,但 `<end_effector>` 應該指向**獨立的 group**(夾爪),不是直接指 arm group。

**解**:
- 對 portfolio 教學版本:WARN 不影響規劃,可忽略
- 真正要寫 gripper:Phase 20B 的 SRDF 加 `<group name="gripper">` 然後 `<end_effector name="tool" parent_link="tool0" group="gripper"/>`

### ⚠️ 雷 6:MoveIt 配 fake controller,真執行就 idle 不動

**症狀**:`arm.execute(plan)` 回 SUCCESS,但 RViz 看手臂不動 / `/joint_states` 維持 0。

**原因**:`moveit_simple_controller_manager/MoveItSimpleControllerManager` 配 `FollowJointTrajectory` action,會 spawn 一個 dummy action server。這個假 server 接 trajectory 但實際**不送 joint command**,只回 SUCCESS。

**解**:本章只 plan 不 execute(教學 plan API)。實機要把 controller 換成 ros2_control 的 `joint_trajectory_controller`(Phase 18 ros2_control 教過)。

---

## 🎯 學到的關鍵概念

- **高階大腦 (`MoveGroupInterface`)**：這是我們在 C++ 程式中與 MoveIt 溝通的最高階代理人。實體化它時指定的名稱（如 `"arm"`），必須與你在 SRDF 中定義的 Planning Group 名字一模一樣。
- **最安全的指令 (`setNamedTarget`)**：這是最穩定的規劃方式。直接呼叫 SRDF 裡預先記好的安全姿態（例如 `home` 或 `ready`），因為沒有牽涉到複雜的逆向運動學，成功率幾乎是 100%。
- **絕對的關節控制 (`setJointValueTarget`)**：當你確切知道每一軸馬達該轉到哪個角度時使用。這跳過了末端點空間計算，只要指定的角度沒有互相穿透，通常都能輕易算出路徑。
- **充滿變數的空間座標 (`setPoseTarget`)**：在業界最常用但也最常惹禍。當你要求夾爪末端移動到 `(X, Y, Z)` 時，MoveIt 會在背景瘋狂運算逆向運動學 (IK) 來推導關節角度。很容易因為超出手臂長度或遇到奇異點 (Singularity) 而徹底失敗。
- **軌跡的密度 (`joint_trajectory.points`)**：這決定了你的手臂動作是平滑還是卡頓。OMPL 演算法算出路徑後，會產生一連串的「中繼點」。點數越多，動作越細膩，但也越吃通訊頻寬。
- **算力指標 (`planning_time_`)**：觀察演算法花了多少時間才解開這道路徑謎題。如果一個簡單的移動每次都要花上幾秒鐘，通常代表你的場景太擁擠，或是碰撞檢查的精細度設得太高了。
- **ROS 2 的參數緊箍咒 (`automatically_declare_parameters_from_overrides`)**：這是 ROS 2 特有的安全機制（嚴格宣告）。MoveIt 為了讀取深層嵌套的 YAML 結構，我們必須在 NodeOptions 強制開啟這個後門，否則底層的 IK Solver 會全部失效。
- **不可或缺的三神兵 (Description Params)**：你的應用程式 Node 想要呼叫 MoveIt，就必須在 Launch 啟動時把 `robot_description` (URDF 外觀)、`semantic` (SRDF 碰撞) 與 `kinematics` (運動學公式) 這三份關鍵文件掛載進去，缺一不可。

---

## 🌟 進階挑戰

1. **真的執行軌跡**:`arm.execute(plan)` 跑完,看 RViz 內手臂動
2. **加 gripper group**:寫 `gripper.macro.xacro` 加進 Phase 20B URDF,SRDF 加 `<group name="gripper">`,demo 內 `gripper.setNamedTarget("open" / "closed")`
3. **Cartesian path interpolation**:用 `arm.computeCartesianPath()` 規劃直線運動(不是 OMPL 的曲線)
4. **碰撞物件**:`PlanningSceneInterface::addCollisionObjects()` 加 box / cylinder,看 OMPL 自動避開
5. **接 ros2_control**:Phase 18 教的 trajectory controller plugin 換上來,手臂真的會動

---

## 🔗 下一步

- **Phase 21B MoveIt Setup Assistant**(等你本機)— GUI wizard 自動產 moveit_config,比手寫 4 個 yaml 快很多
- **Phase 23B Pick & Place**(等你本機)— 加 gripper、抓物件、視覺整合
- **Capstone B**(等)— 完整 pick-and-place + Gazebo 模擬
- **[Phase 18 ros2_control](../phase-18-ros2-control/)** — 真實機 controller 替代 fake controller

---

## 📁 完整檔案結構

```
phase-22B-moveit-cpp/
├── README.md
├── code/
│   ├── my_arm_moveit_config/                  ← MoveIt 設定 package
│   │   ├── package.xml
│   │   ├── CMakeLists.txt
│   │   ├── config/
│   │   │   ├── kinematics.yaml                ← KDL IK plugin
│   │   │   ├── ompl_planning.yaml             ← RRTConnect planner
│   │   │   ├── joint_limits.yaml              ← v/a 上限
│   │   │   └── moveit_controllers.yaml        ← fake trajectory controller
│   │   └── launch/
│   │       └── move_group.launch.py           ← rsp / jsp / move_group
│   └── my_arm_moveit_demo/                    ← MoveGroupInterface demo
│       ├── package.xml
│       ├── CMakeLists.txt
│       ├── src/
│       │   └── plan_demo.cpp                  ← 4 個 plan demo
│       └── launch/
│           └── plan_demo.launch.py            ← include moveit config + plan_demo Node
└── images/                                    ← (之後補:RViz MotionPlanning 截圖)
```
