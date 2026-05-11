# Phase 22A:Nav2 入門 — 全套自主導航

> **Nav2** 是 ROS 2 的自主導航標準 stack。本章用 `nav2_bringup` 啟動 8 個 lifecycle node(map_server / amcl / controller / planner / smoother / behavior / bt_navigator),載入靜態地圖跑 localization + path planning。

**學完你會**:
- 認識 Nav2 8 個核心 lifecycle node 的角色與啟動順序
- 用 `nav2_bringup/bringup_launch.py` 一條指令啟動全套
- 寫 `nav2_params.yaml` 設定 costmap、planner、controller plugin
- 用 `ros2 action send_goal /navigate_to_pose` 從 CLI 下導航目標
- 看穿 `base_link` vs `base_footprint` 這個 turtlebot3 必踩的雷

**前置**:
- [Phase 17 Gazebo](../phase-17-gazebo/) — simulator
- [Phase 21A SLAM](../phase-21A-slam-toolbox/) — 預先建出地圖(本章直接用 ship 的 4×4m map)
- [Phase 09 Lifecycle](../phase-09-executors-lifecycle-composition/) — Nav2 完全靠 lifecycle 管理啟動

**產出**:
- [`config/nav2_params.yaml`](code/my_nav2_demo/config/nav2_params.yaml) — Nav2 完整設定(350 行)
- [`maps/empty_4x4.yaml`](code/my_nav2_demo/maps/empty_4x4.yaml) + `empty_4x4.pgm` — 4×4m 圍牆地圖
- [`launch/nav2_demo.launch.py`](code/my_nav2_demo/launch/nav2_demo.launch.py) — Gazebo + Nav2 整合

**環境**:☁️ TheConstructSim 推薦(實際看 RViz 規劃路徑) ｜ 💻 WSL2(可驗證 lifecycle activation,實際導航受 GPU 限制)

> 同 Phase 21A,**WSL 沒 GPU,Nav2 各 node 都 active 但 amcl 仍會 message filter drop**,不易跑出真實導航。本章已驗證「8 個 node 完整 Configure → Activate」結構正確,實機 / 雲端可直接照跑出導航。

---

## 🗂️ Nav2 名詞速查(看這章 yaml 之前必懂)

Nav2 文件裡這些名詞會密集出現,先看完這個表再讀後面就不會迷路:

| 名詞 | 是什麼 | 你會在哪看到 |
|------|--------|-------------|
| **Costmap** | 一張「**走過會撞 / 不能走**」的地圖,每格有 0–254 分數 | 90% YAML 都在調 costmap |
| **Global costmap** | 大範圍、靜態為主、用來規劃**長路徑** | `global_costmap:` 區段 |
| **Local costmap** | 機器人周圍幾米的小框、即時更新、用來**避開突然出現的障礙** | `local_costmap:` 區段 |
| **Inflation** | 障礙物外圍的「**緩衝圈**」,讓機器人不要貼著牆走 | costmap 內 `inflation_layer` |
| **Planner** | 看 global costmap,規劃「**從這裡到目標的長路徑**」 | `planner_server:` |
| **Controller** | 看 local costmap,負責「**目前這幾秒方向盤怎麼打**」(連續發 cmd_vel) | `controller_server:` |
| **AMCL** | Adaptive Monte Carlo Localization — 用粒子濾波在**已知地圖**上定位 | `amcl:` |
| **Behavior Tree(BT)** | 用 XML 編排「先嘗試 A,失敗就 B,還失敗就 recovery」這種行為流程 | `bt_navigator:` |
| **Lifecycle node** | Phase 09 教過的那個五狀態 Node — Nav2 全部 8 個 node 都是 | `lifecycle_manager:` |

### 一句話講完 Nav2 流程

```
你下 goal → BT 決定步驟 → Planner 算長路徑 → Controller 發 cmd_vel
                              ↑                    ↑
                         看 global costmap    看 local costmap
                                                   ↑
                                            AMCL 提供「我在哪」
```

**90% Nav2 工作就是調 yaml 內這幾個東西的參數**。本章會逐個帶你過。

---

## 🤔 為什麼這章重要

Nav2 是 ROS 2 移動機器人的**事實標準**。會用 Nav2 = 會做掃地機 / AGV / 配送車。

業界 ROS robotics 工程師 80% 工作都在跟 Nav2 打交道:
- 調 costmap 範圍、解析度、inflation
- 換 planner(NavfnPlanner / SmacPlanner / Theta*)
- 換 controller(DWB / RPP / MPC / GracefulMotion)
- 自訂 BT plugin 加新行為(Phase 23A)
- 調 amcl 粒子數、init pose

---

## 🏗️ Nav2 8 大 component(lifecycle node)

```
┌──────────────────────── lifecycle_manager_localization ──────────────────────┐
│   map_server          amcl                                                   │
│   讀 /map.yaml 發 /map  讀 /scan + /map 算位姿,發 amcl_pose                  │
└──────────────────────────────────────────────────────────────────────────────┘
┌──────────────────────── lifecycle_manager_navigation ────────────────────────┐
│  controller_server    planner_server         smoother_server                 │
│  ├─ DWB plugin        ├─ GridBased plugin    ├─ simple_smoother              │
│  └─ local_costmap     └─ global_costmap                                      │
│                                                                              │
│  behavior_server      bt_navigator           waypoint_follower               │
│  ├─ spin              讀 navigate_to_pose                                    │
│  ├─ backup            BT.cpp 樹編排所有       讀目標序列依序送 nav            │
│  ├─ drive_on_heading                                                         │
│  ├─ assisted_teleop                                                          │
│  └─ wait                                                                     │
└──────────────────────────────────────────────────────────────────────────────┘
```

兩個 lifecycle manager 把 8 個 component 分成兩組,各自 configure→activate→deactivate→cleanup。
這個架構 = **Nav2 的精華**:每個元件可以獨立 cycle,plugin 可以 hot-swap,出事可以單獨重啟。

---

## 💻 重點檔案

### 1. config/nav2_params.yaml — 350 行設定

**直接複製 nav2_bringup 的 default + 改 base_frame**(雷 1)。重點區段:

```yaml
amcl:
  ros__parameters:
    base_frame_id: "base_footprint"      # turtlebot3 的 root link

local_costmap:
  local_costmap:
    ros__parameters:
      robot_base_frame: base_footprint   # 同上,本來預設是 base_link

planner_server:
  ros__parameters:
    planner_plugins: ["GridBased"]
    GridBased:
      plugin: "nav2_navfn_planner/NavfnPlanner"  # 經典 Dijkstra/A*

controller_server:
  ros__parameters:
    FollowPath:
      plugin: "dwb_core::DWBLocalPlanner"        # Dynamic Window Approach

bt_navigator:
  ros__parameters:
    default_nav_to_pose_bt_xml: "navigate_to_pose_w_replanning_and_recovery.xml"
```

### 2. maps/empty_4x4.yaml + empty_4x4.pgm

```yaml
image: empty_4x4.pgm
resolution: 0.05            # 5cm/cell
origin: [-2.0, -2.0, 0.0]   # PGM 左下對應 world (-2, -2)
occupied_thresh: 0.65
free_thresh: 0.196
```

PGM 80×80 px,邊框 1 格畫黑(=牆),內部全白(=free)。
對應 Phase 17 的 4×4m 圍牆 world,跑起來機器人位置會跟地圖對得上。

### 3. launch/nav2_demo.launch.py

```python
return LaunchDescription([
    # 1. Gazebo + tb3
    IncludeLaunchDescription(...headless_demo.launch.py),

    # 2. Nav2 等 10 秒(等 Gazebo + tf 穩)
    TimerAction(period=10.0, actions=[
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(...bringup_launch.py),
            launch_arguments={
                'use_sim_time': 'true',
                'map': map_yaml,
                'params_file': nav2_params,
                'autostart': 'true',
            }.items())])
])
```

`autostart: true` 讓 lifecycle manager 自動 configure → activate,不用手動 `ros2 lifecycle set`。

---

## 🚀 完整 Demo 流程

### Step 1:部署 + build

```bash
rm -rf ~/ros2_ws/src/my_nav2_demo
cp -r /mnt/d/ros_learn/ros2-learning-notes/phase-22A-nav2-basics/code/my_nav2_demo \
      ~/ros2_ws/src/my_nav2_demo
source /opt/ros/humble/setup.bash
cd ~/ros2_ws && colcon build --packages-select my_nav2_demo
```

### Step 2:啟動

```bash
export TURTLEBOT3_MODEL=burger
ros2 launch my_nav2_demo nav2_demo.launch.py
```

啟動後預期:
- t=0:Gazebo + tb3
- t=10s:Nav2 stack 啟動
- t=10–15s:8 個 lifecycle node configure
- t=15–20s:全部 activate

### Step 2.5:☁️ TheConstructSim 步驟(推薦 — 真的能跑出導航)

**雲端是這章的最佳環境**(WSL 沒 GPU,Nav2 active 但車跑不動,雷 4)。

```bash
# 1. 雲端 ROSject terminal
cd ~/ros2_ws/src
git clone https://github.com/gino07172002/ros2-learning-notes.git
cp -r ros2-learning-notes/phase-22A-nav2-basics/code/my_nav2_demo .

# 2. 確認 Nav2 + turtlebot3 已裝
ros2 pkg list | grep -E 'turtlebot3|nav2'
export TURTLEBOT3_MODEL=burger

# 3. Build + 跑
cd ~/ros2_ws
colcon build --packages-select my_nav2_demo
source install/setup.bash
ros2 launch my_nav2_demo nav2_demo.launch.py

# 4. 開 Tools → Graphical Tools 跳出的 RViz 內:
#    - 點 "2D Pose Estimate" 在地圖上設機器人初始位置
#    - 點 "Nav2 Goal" 在地圖任一點 → 車真的會規劃路徑跑過去
```

**雲端能完整驗證的事**(WSL 不能):
- 機器人沿 global path 走
- 動態避障
- BT navigator 真的走完 NavigateToPose action

> 💡 **找不到 Nav2 stack** → 用 TheConstruct 的 **"ROS 2 Navigation"** 課程附帶 ROSject(Nav2 預裝齊全)。

---

### Step 3:驗證 Nav2 lifecycle 全部 active(WSL 驗證過)

```
[component_container_isolated-4] [lifecycle_manager_localization]: Configuring map_server
[component_container_isolated-4] [lifecycle_manager_localization]: Configuring amcl
[component_container_isolated-4] [lifecycle_manager_localization]: Activating map_server
[component_container_isolated-4] [lifecycle_manager_localization]: Activating amcl
[component_container_isolated-4] [lifecycle_manager_localization]: Managed nodes are active   ← ✅
[component_container_isolated-4] [lifecycle_manager_navigation]:    Configuring controller_server
[component_container_isolated-4] [lifecycle_manager_navigation]:    Configuring smoother_server
[component_container_isolated-4] [lifecycle_manager_navigation]:    Configuring planner_server
[component_container_isolated-4] [planner_server]: Configuring plugin GridBased of type NavfnPlanner
[component_container_isolated-4] [lifecycle_manager_navigation]:    Configuring behavior_server
[component_container_isolated-4] [behavior_server]: Configuring spin
[component_container_isolated-4] [behavior_server]: Configuring backup
[component_container_isolated-4] [behavior_server]: Configuring drive_on_heading
[component_container_isolated-4] [behavior_server]: Configuring assisted_teleop
[component_container_isolated-4] [behavior_server]: Configuring wait
... → Activating ... → Managed nodes are active ✅
```

**全部 8 個 lifecycle node 在 WSL 驗證為 Active**:
- map_server / amcl(localization 組)
- controller_server / smoother_server / planner_server / behavior_server / bt_navigator(navigation 組)
- 各自 plugins:NavfnPlanner / DWB / spin / backup / wait

### Step 4:從 CLI 下導航目標

啟動後另開 terminal:

```bash
# 設初始位姿(Nav2 開機時 amcl 不知道車在哪)
ros2 topic pub --once /initialpose geometry_msgs/PoseWithCovarianceStamped \
  '{header: {frame_id: "map"}, pose: {pose: {position: {x: 0.0, y: 0.0, z: 0.0}, orientation: {w: 1.0}}}}'

# 下導航目標(車自己規劃路徑跑去 (1.5, 0))
ros2 action send_goal /navigate_to_pose nav2_msgs/action/NavigateToPose \
  '{pose: {header: {frame_id: "map"}, pose: {position: {x: 1.5, y: 0.0, z: 0.0}, orientation: {w: 1.0}}}}' \
  --feedback
```

預期 feedback:
```
Feedback:
  current_pose: ...
  navigation_time: 5.2s
  distance_remaining: 1.2m
```

WSL 沒 GPU 時可能會看到 controller 跑慢、車不動,但 action 結構是對的(雷 4)。

### Step 5:RViz 視覺操作(雲端 ROSject 推薦)

```bash
# 雲端 ROSject 已預先設定 RViz config
ros2 launch my_nav2_demo nav2_demo.launch.py
# 另開 terminal
ros2 launch nav2_bringup rviz_launch.py
```

RViz 上:
1. 點 "2D Pose Estimate" → 在地圖上點機器人實際位置
2. 點 "2D Goal Pose" → 在地圖上點目標,看 planner 畫綠線、看 controller 帶車跑

---

## 🐛 常見雷

### ⚠️ 雷 1:**`robot_base_frame: base_link` 預設,但 turtlebot3 用 base_footprint**

**症狀(實測)**:
```
[local_costmap.local_costmap]: Timed out waiting for transform from base_link to odom
to become available, tf error: Invalid frame ID "base_link" passed to canTransform
argument source_frame - frame does not exist
```
local_costmap / global_costmap 永遠等不到 TF,Nav2 無法產生 costmap → 規劃失敗。

**原因**:nav2_bringup 預設 `nav2_params.yaml` 寫死 `robot_base_frame: base_link`,但 turtlebot3 SDF 的 root 是 `base_footprint`。

**解**:`sed -i 's|robot_base_frame: base_link|robot_base_frame: base_footprint|g' nav2_params.yaml`(本章 yaml 已改)。

或:**用 turtlebot3_navigation2 的 nav2_params**(他們已經改好了)。

### ⚠️ 雷 2:Nav2 在 Gazebo 啟動完前就跑,8 個 node 全部 timeout

**症狀**:Nav2 啟動,所有 lifecycle node 卡在 Configuring,最終 timeout。

**原因**:Gazebo 起來要 15+s,期間沒 `/scan` 沒 `/tf`,Nav2 各 node configure 時找不到必要訊息。

**解**:`TimerAction(period=10.0)` 延遲 Nav2 啟動。本章 launch 已用。

### ⚠️ 雷 3:`use_sim_time: true` 沒傳給 Nav2

**症狀**:Nav2 跑了,但 controller 永遠收不到 odom,看到 `extrapolation into the past`。

**原因**:跟 Phase 21A 同樣的時間源混用。Gazebo 發 sim time,Nav2 用 wall time → 對不上。

**解**:**bringup_launch.py 一定要傳 `use_sim_time: 'true'`**:
```python
launch_arguments={'use_sim_time': 'true', ...}.items()
```

`bringup_launch.py` 內部會把這個值傳給所有 nav2 node 的 yaml params。

### ⚠️ 雷 4:WSL 沒 GPU,amcl 跟 SLAM 一樣 drop scan,規劃出但車不動

**症狀**:
```
[amcl]: Message Filter dropping message: frame 'base_scan' at time XX
        for reason 'the timestamp on the message is earlier than all the data in the transform cache'
```
amcl 顯示 active,但收不到有效 scan → localization 不準。下了 goal,planner 算出路徑,但 controller 拿不到 odom 跟 cmd_vel 速度,車**幾乎不動或亂晃**。

**解**:
1. **雲端 / 實機 GPU 跑**(本章主推)
2. WSL 內把 Gazebo `<real_time_factor>0.3</real_time_factor>`,sim time 跑 30%(Phase 17 已加)
3. WSL 內降低 nav2 各 server 頻率(`controller_frequency: 5.0` 之類),CPU 才追得上

### ⚠️ 雷 5:沒設 `/initialpose`,amcl 一直 uniform distribution

**症狀**:Nav2 啟動後車站在原地,localization 看 RViz 是滿滿粒子,沒收斂。

**原因**:amcl **第一次啟動不知道車在哪**,粒子均勻散在整個 map。需要外部訊號告訴它「初始位姿大概在 (0,0)」。

**解**:啟動後第一件事:
```bash
ros2 topic pub --once /initialpose geometry_msgs/PoseWithCovarianceStamped \
  '{header: {frame_id: "map"}, pose: {pose: {position: {x: 0.0, y: 0.0, z: 0.0}, orientation: {w: 1.0}}}}'
```
或在 RViz 點「2D Pose Estimate」。實機可以用 NFC tag / QR code 自動定位。

### ⚠️ 雷 6:planner / controller plugin 沒裝

**症狀**:啟動時看到 `Failed to create planner plugin GridBased`,Nav2 部分組件 fail to configure。

**原因**:Nav2 用 pluginlib 載 planner / controller。預設裝了 NavfnPlanner / DWB,但其他(SmacPlanner、TebLocalPlanner、RPP)需要額外 apt install。

**解**:`apt list --installed | grep nav2-` 確認你需要的 plugin 都有。沒有的 `apt install ros-humble-nav2-<plugin_name>`。

### ⚠️ 雷 7:Gazebo + slam_toolbox + Nav2 的 map 衝突

**症狀**:同時跑 SLAM 跟 Nav2,看到 `/map` 一直跳變,Nav2 不知道用哪個。

**原因**:slam_toolbox 跟 nav2_map_server **都會發 `/map` topic**。同時跑 = 衝突。

**解**:**Nav2 用靜態 map(本章)時不要再開 SLAM**。如果要邊建邊導航,用 `nav2_bringup/slam_launch.py` (它整合 slam_toolbox + Nav2 + 共用 map)。

---

## 🎯 學到的關鍵概念

| 概念 | 一句話 |
|------|------|
| Nav2 = 8 個 lifecycle node | localization 2 + navigation 6,各組獨立 lifecycle manager |
| `bringup_launch.py` 是 Nav2 入口 | 一條 launch 把 8 個 node 都帶起來 |
| `autostart: true` | 讓 lifecycle manager 自動 configure→activate,不用手動 |
| `robot_base_frame` 因 robot 而異 | turtlebot3 是 `base_footprint`,別信預設 |
| `/initialpose` 必下 | amcl 啟動不知道車在哪,粒子不收斂 |
| `bt_navigator` 用 BT.cpp | 規劃失敗時自動 retry / spin / backup,可自訂 BT |
| WSL GPU 不足 = 結構過、效能不過 | 學習用雲端,實機部署 Jetson |

---

## 🌟 進階挑戰

1. **換 planner**:把 GridBased 換成 SmacPlanner (`nav2_smac_planner/SmacPlannerHybrid`),同 goal 看路徑差異
2. **換 controller**:DWB 換成 RPP (`nav2_regulated_pure_pursuit_controller`),弧線跟得更平
3. **加 dynamic obstacle**:在 Gazebo 加 actor 行人,看 local_costmap 即時 inflation + controller 避開
4. **多目標巡邏**:用 `waypoint_follower` action 連續送 5 個目標,車自動跑完整圈
5. **錄 ROS bag**:`ros2 bag record /tf /scan /odom /amcl_pose /plan /local_costmap/costmap` debug 用

---

## 🔗 下一步

- **Phase 23A Nav2 BT plugin** — 寫自己的 Behavior Tree node 加進 bt_navigator
- **[Phase 18 ros2_control](../phase-18-ros2-control/)** — Nav2 發 cmd_vel,後面靠 ros2_control 真的轉輪子
- **Capstone A** — 結合 SLAM + Nav2 完整 demo

---

## 📁 完整檔案結構

```
phase-22A-nav2-basics/
├── README.md
├── code/
│   └── my_nav2_demo/
│       ├── package.xml
│       ├── CMakeLists.txt
│       ├── config/
│       │   └── nav2_params.yaml          ← Nav2 完整設定(350 行)
│       ├── maps/
│       │   ├── empty_4x4.pgm             ← 4×4m 圍牆地圖
│       │   └── empty_4x4.yaml            ← map_server 設定
│       └── launch/
│           └── nav2_demo.launch.py       ← Gazebo + Nav2 整合
└── images/                              ← (之後補:RViz 路徑規劃截圖)
```
