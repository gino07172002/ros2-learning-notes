# Portfolio Highlights — ROS 2 Learning Notes

> 這份文件給急著看重點的人(招聘方、code reviewer、自己回頭看)。
> 完整章節結構在 [README.md](README.md) / [ROADMAP.md](ROADMAP.md)。

---

## TL;DR

> 從零開始學 ROS 2 Humble 的實戰筆記。**32 個獨立 phase**,每章可單獨編譯執行,完整覆蓋從通訊基礎、系統設計、機器人形體、SLAM/Nav2 自主導航、機械手臂 MoveIt、CI/CD,到 Docker 化部署。**所有 demo log 都是真實 WSL2 跑出來的輸出,踩過的雷都記下來**。

- **語言**:C++(主)+ Python(對照)+ Markdown
- **平台**:ROS 2 Humble(LTS)on WSL2 Ubuntu 22.04 / TheConstructSim 雲端
- **套件**:Nav2 / slam_toolbox / robot_localization / MoveIt 2 / ros2_control / pluginlib / BT.cpp / Gazebo Classic
- **產出**:31 個 ROS package,2 個 Capstone 整合作品,1 個 Docker 化交付鏡像
- **連結**:[GitHub repo](https://github.com/gino07172002/ros2-learning-notes)

---

## 🌟 三個最能講故事的章節

如果你只想看 3 個 phase,看這 3 個。

### 1. [Capstone Final — Docker 化全套 Mobile Robot](phase-Capstone-Final/) 🏁

**展示**:整個 repo 的最終 deliverable。Multi-stage Dockerfile 把整套 ROS 2 mobile robot stack(Gazebo + Nav2 + 自訂 BT plugin + Auto navigator)打包成 1.26GB image,**`docker compose up` 一條指令任何機器都能跑**。

**為什麼強**:
- **整合**了前面 4 個獨立 phase(Phase 17 Gazebo + 22A Nav2 + 23A BT plugin + Capstone A)
- **完整 build context 設計**:repo root 當 context + .dockerignore 白名單,只把需要的 phase 帶進(避免 200MB 全傳)
- **DDS in Docker 的兩個經典雷都修了**:`network_mode: host` 解 multicast、`ipc: shareable` 解 SHM transport
- **接 GitHub Actions / GHCR 即可**push image 給實機 Pi/Jetson 直接 `docker pull`

**實測結果**:`docker compose up` 後 container 啟動,Nav2 lifecycle 全 active,日誌:
```
[capstone-final] exec: ros2 launch capstone_a capstone_a.launch.py
[lifecycle_manager_localization]: Managed nodes are active   ← ✅
[waypoint_follower]: Created waypoint_task_executor : wait_at_waypoint
```

### 2. [Phase 20A — Odometry + robot_localization EKF 融合](phase-20A-odometry-ekf/)

**展示**:寫兩個假感測器(輪式 odometry + IMU),餵給 robot_localization EKF,跟「真值」(知道是定速圓周運動)比較。**用真實數據證明 EKF 比單一 sensor 準 8 倍**。

**為什麼強**:
- 不靠 Gazebo / 不靠 GPU,**純文字驗證 EKF 工作**
- 設計了 `comparator.cpp` 訂閱 wheel/EKF 並對比真值,輸出量化誤差
- 教學重點清楚:EKF 在 covariance 矩陣怎麼選、`two_d_mode` 為什麼必開、缺 `imu_link → base_link` TF 是無聲失敗

**實測結果**(16 秒繞圓):
```
t=  5s | WHEEL Δ=0.31m  EKF Δ=0.06m  →  EKF 5×
t= 10s | WHEEL Δ=1.00m  EKF Δ=0.18m  →  EKF 5.5×
t= 16s | WHEEL Δ=1.74m  EKF Δ=0.22m  →  EKF 8× ✅
```

### 3. [Phase 14 — Capstone 1 ApproachController](phase-14-capstone-1/)(Part 3 收尾)

**展示**:整合 Phase 08-13 — 自訂 Action(Approach.action) + LifecycleNode + Launch 自動化 lifecycle transition + 5 個 gtest 單元測試。

**為什麼強**:
- 一個 Node 同時當 6 個角色(Lifecycle + Action server + Service + Subscriber + Publisher + Timer)
- launch 用 event_handler 自動 configure → activate,**不依賴使用者手動 lifecycle set**
- 純邏輯 class 抽出來給 gtest 驗,業界等級的設計

---

## 💎 最有故事的 6 條雷(從 60+ 條中精選)

### 雷 1:**Docker host network 還是收不到 BestEffort sensor data** — Phase 24

明明 `network_mode: host` 解了 multicast,topics 看得到、reliable 訊息收得到,但 BestEffort 的 `/lidar_points` 永遠是空的。

**根因**:Docker container 雖然共用 network namespace,但 IPC namespace 還是隔離的,FastRTPS 的 SHM transport 跨 container 走不通,fall back 到 loopback UDP 又被 BestEffort 不重傳特性吃掉封包。

**解**:`ipc: shareable` + `ipc: service:<另一服務>`,讓兩個 container 共用 IPC namespace。

→ 這個雷 [Phase 24 README](phase-24-docker/) 跟 [Capstone Final](phase-Capstone-Final/) 都有完整解析。**業界 Docker + ROS 2 教學少有人講到這層**。

### 雷 2:**沒 imu_link → base_link TF,EKF 默默丟掉所有 IMU 訊息** — Phase 20A

EKF 啟動成功、`/odometry/filtered` 在發、沒任何 error,但 `yaw` 永遠是 0。看半天才發現:**EKF 找不到 `imu_link → base_link` 變換 → 整個 IMU 被無聲忽略**,沒 warning。

**解**:launch 加 `static_transform_publisher base_link → imu_link`(實機要設真實 IMU 安裝偏移)。

→ 這個是實機部署 Nav2 stack 最常踩的雷之一。[Phase 20A README](phase-20A-odometry-ekf/) 雷 1。

### 雷 3:**MoveIt 2 獨立 Node 的「三份 description params」** — Phase 22B

`MoveGroupInterface` 構造時要從 ROS parameter 讀 `robot_description` / `robot_description_semantic` / `robot_description_kinematics`。**這些 param 設在 move_group 上不會自動共享給其他 Node** — 每個 Node 有自己的 parameter namespace。

我寫 plan_demo Node 第一次跑就炸:
```
[FATAL] [move_group_interface]: Unable to construct robot model.
```

**解**:plan_demo Node 啟動時也明確帶上同樣 3 份 params + `automatically_declare_parameters_from_overrides=true`(讀 nested yaml)。

→ ROS 1 → ROS 2 移植版最常踩,[Phase 22B README](phase-22B-moveit-cpp/) 雷 1+2。

### 雷 4:**Nav2 預設 robot_base_frame: base_link 但 turtlebot3 用 base_footprint** — Phase 22A

跑 Nav2 啟動完美,但 local_costmap 永遠在報:
```
Timed out waiting for transform from base_link to odom
```

**根因**:nav2_bringup 預設 `nav2_params.yaml` 寫 `robot_base_frame: base_link`,但 turtlebot3 SDF root 是 `base_footprint`。
nav2 各組 (controller_server / planner_server / global_costmap / local_costmap) 都要改。

**解**:`sed -i 's|robot_base_frame: base_link|robot_base_frame: base_footprint|g' nav2_params.yaml`(5 個位置都會改)。

→ [Phase 22A README](phase-22A-nav2-basics/) 雷 1。

### 雷 5:**StatefulActionNode 三態 vs 一般 ActionNode** — Phase 30

寫 `GoToCharger` 一開始繼承 `BT::ActionNodeBase`,以為一個 `tick()` 回 RUNNING / SUCCESS / FAILURE 就完事 — 然後 BT 就一直 spam tick(),CPU 100%。

**根因**:`BT::ActionNodeBase` 是同步呼叫,每 tick 都要返回終態。長時間動作要繼承 `BT::StatefulActionNode`,實作 3 個方法:
- `onStart()` — 進入動作(只呼叫 1 次)
- `onRunning()` — 還在跑(每 tick)
- `onHalted()` — 上層 abort,要清資源

**還有第二雷**:`onHalted()` 沒實作 — 導致 Halt 後再次 tick 時,`onStart()` 不會被觸發(因為內部 state 沒 reset),動作直接卡住。**onHalted 必須清 internal state**。

→ [Phase 30 README](phase-30-nav2-bt-advanced/) 雷 1+5 詳解,是 BT.cpp v3 寫長 action 的關鍵。

### 雷 6:**WSL2 background process lifetime 問題** — 多章踩到

WSL 開的 ros2 launch 即使 `setsid` / `nohup` detach,**wsl 命令結束時背景 process 仍可能被 systemd-user-session 收掉**。`background tool` 模式更不穩,常 SIGKILL exit 9。

**最終解**:同步 timeout 命令(`timeout 30 ros2 launch ...`)是最穩的驗證方式,demo 設計成「自己 timeout 結束」的 launch(用 `TimerAction` + `ExecuteProcess` 帶超時)。

→ HANDOFF.md 記下來,影響後面所有 Track A/B 章節的測試策略。

---

## 🗺️ 完整技術棧覆蓋

```
通訊層:      Pub/Sub | Service | Action | QoS | DDS Discovery (Multicast / Server)
治理層:      Parameters | Lifecycle | Composition | Launch | gtest + launch_testing
協議設計:    Custom .msg / .srv / .action 自訂 interface
形體描述:    URDF | xacro macro | SRDF | TF2 broadcaster/listener | static/dynamic TF
硬體抽象:    ros2_control | hardware_interface | controller_manager
擴充機制:    pluginlib | BT.cpp v3 (Behavior Tree) | rclcpp_components
模擬環境:    Gazebo Classic 11 | turtlebot3_gazebo | spawn_entity
感測融合:    robot_localization EKF | nav_msgs/Odometry | sensor_msgs/Imu
建圖定位:    slam_toolbox | nav2_amcl
自主導航:    Nav2 (8 lifecycle nodes) | DWB controller | NavfnPlanner | Behavior tree
機械手臂:    MoveIt 2 | MoveGroupInterface | KDL kinematics | OMPL RRTConnect
生產化:      Docker multi-stage | docker-compose | GitHub Actions CI/CD
```

---

## 📊 實際投入

- **Phase 數**:33(20 個前期 + 12 個本次補,加 1 個 Capstone Final)
- **Code 行數**:~5500 行 C++ + ~3500 行 Python + ~24000 行 Markdown
- **驗證標準**:每章 README 內「驗證過」段都是 WSL 真跑的輸出 + 雷區條目都是實際踩過修好的
- **gtest 測試**:Phase 12 + Capstone 1(5 個)+ Phase 23A(4 個)+ Phase 30(6 個)= 約 16 個 gtest case
- **Docker images**:capstone1:latest(Phase 24)+ phase20:latest(Phase 20)+ capstone-final:latest(整套 1.26GB)

---

## 🔗 跟它互動的方式

```bash
# 從零開始,clone repo
git clone https://github.com/gino07172002/ros2-learning-notes.git
cd ros2-learning-notes

# 跳到任何一章,獨立編譯執行(以 Phase 22B MoveIt 為例)
cp -r phase-22B-moveit-cpp/code/* ~/ros2_ws/src/
cd ~/ros2_ws
colcon build --packages-select my_arm_moveit_config my_arm_moveit_demo
source install/setup.bash
ros2 launch my_arm_moveit_demo plan_demo.launch.py

# 或直接 docker 跑 Capstone Final
docker compose -f phase-Capstone-Final/code/docker-capstone-a/docker-compose.yml up
```

---

## ⏭️ 還沒做的(刻意擱置)

| 章節 | 為什麼擱置 |
|------|----------|
| Phase 21B MoveIt Setup Assistant | 純 GUI wizard,本機操作 |
| Phase 23B Pick & Place | 視覺主導,Gazebo + 視覺驗證 |
| Capstone B 機械手臂 | 視覺主導,完整 demo 要看抓物件 |
| Phase 27 部署實機 | 沒 Pi/Jetson 硬體 |

---

## 🤝 給想用這個 repo 學 ROS 2 的人

- **每章獨立可學**,不必照順序做完所有前置
- **雙環境**:☁️ TheConstructSim(免裝即用)/ 💻 本機 WSL2(進階作業)
- **每章雷區是寶**:這些不是教科書會寫的,全是實際踩到的工程細節
- 有問題開 [GitHub issue](https://github.com/gino07172002/ros2-learning-notes/issues)
