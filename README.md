# ROS 2 學習筆記（C++）

從零開始學 ROS 2 Humble 的實戰筆記。每個 phase 是獨立的專案資料夾,自帶完整可編譯的 code 與說明,**可跳讀、可單獨執行**。

> **🎯 急著看重點?** 看 [`PORTFOLIO.md`](PORTFOLIO.md) — 30 秒內知道這個 repo 是什麼、技術棧、3 個最強章節、5 條最有故事的雷。

---

## 🌟 Highlights(整個 repo 最值得看的東西)

如果你只有 5 分鐘:

| 看哪個 | 為什麼 |
|--------|--------|
| 🏁 [**Capstone Final**](phase-Capstone-Final/) | 整個 repo 的最終 deliverable。Docker 化 Mobile Robot,1.26GB image,`docker compose up` 一鍵跑 |
| 🎯 [**Capstone A**](phase-CapstoneA-mobile/) | Mobile Robot 整合:Gazebo + Nav2 + 自訂 BT plugin + auto_navigator,實機部署等級的 launch |
| 🎯 [**Capstone 1**](phase-14-capstone-1/) | ApproachController:6 角色 LifecycleNode + 自動 launch + 5 個 gtest |
| 📊 [**Phase 20A EKF**](phase-20A-odometry-ekf/) | 用真實數據證明 EKF 比單一 sensor 準 8 倍(WHEEL Δ=1.74m vs EKF Δ=0.22m) |

---

## 🗺️ 完整章節結構

> 章節依「**學習層次**」分成幾個 Part。每個 Part 內各章可跳讀,但 Part 之間有遞進關係 — 下一個 Part 假設你能用前一個 Part 的東西。

### 📖 Part 1: 通訊基礎 — 讓 Node 能講話
> 學完 Part 1 你能用現成的 ROS 訊息類型寫出 Pub/Sub/Service Node。

| Phase | 主題 | 學到什麼 | 🐍 Py |
|-------|------|----------|------|
| [01](phase-01-cloud-env-first-publisher/) | 雲端環境 + 第一支 Publisher | 建 ROS 2 套件、用 colcon 編譯、寫 Publisher 讓車子前進 | ✅ |
| [02](phase-02-communication-concepts/) | **ROS 2 設計哲學** | DDS / 節點發現 / 訊息序列化 / 與 MQTT/gRPC/ROS 1 對比 | — |
| [03](phase-03-subscriber-lidar-brake/) | Subscriber + 光達避障 | 寫 Subscriber、QoS(SensorDataQoS)、解析 PointCloud2、做避障邏輯 | ✅ |
| [04](phase-04-services-toggle/) | Service Server + 開關 | Service vs Topic、實作 SetBool 服務、雙終端機驗證 | — |

### 🔧 Part 2: 工具與治理 — 看清楚系統 + 微調系統
> 學完 Part 2 你能 debug 不熟的 ROS 系統、調整 Node 行為而不改 code,且能組合既有元件做小作品。

| Phase | 主題 | 學到什麼 | 🐍 Py |
|-------|------|----------|------|
| [05](phase-05-debug-tools/) | Debug 工具集 | rqt_graph 看通訊圖、ros2 bag 錄製/重播、rqt_plot 即時繪圖、rqt_console 集中 log | — |
| [06](phase-06-parameters/) | Parameters(參數系統) | declare/get/on_set callback、YAML 設定檔、rqt_reconfigure GUI 即時調參 | — |
| [🎯 07](phase-07-mini-capstone-1/) | **Mini Capstone 1**:智能煞車車(整合 Param + Service + LiDAR + turtlesim) | 寫一個 Node 同時當 5 個角色、launch file 入門 | — |

### 🏗️ Part 3: 系統設計 — 自己定義協議與架構
> **核心分水嶺**:從「使用現成 ROS 元件」進到「設計自己的 ROS 系統」。學完 Part 3 你能組起一個多節點專案。

| Phase | 主題 | 學到什麼 | 🐍 Py |
|-------|------|----------|------|
| [08](phase-08-custom-interfaces/) | **Custom Interfaces**(自訂訊息) | 定義 .msg / .srv / .action、interface 套件分離、rosidl 生成 C++ class | — |
| [09](phase-09-executors-lifecycle-composition/) | Executors / Lifecycle / Composition | Single vs Multi Executor、CallbackGroup、Lifecycle 五狀態、rclcpp_components | — |
| [10](phase-10-launch-files-basics/) | Launch Files 基礎 | 4 個漸進範例:最小、remap+param、YAML、CLI args | — |
| [11](phase-11-launch-files-advanced/) | Launch Files 進階 | IncludeLaunchDescription、event_handler、條件啟動、namespace 多機器人 | — |
| [12](phase-12-testing/) | 測試(gtest + rclcpp) | 純邏輯單元測試 + rclcpp 整合測試、colcon test、XML 報告 | — |
| [13](phase-13-actions-advanced/) | Actions 進階 | reject/abort/cancel 全套處理、SIGINT 觸發 cancel、Countdown demo | — |
| [🎯 Capstone 1](phase-14-capstone-1/) | **ApproachController**:Lifecycle + Action + Custom Interfaces + Tests 整合 | 6 角色 LifecycleNode + 自動化 launch + 5 個單元測試(GitHub portfolio-ready) | — |

### 🤖 Part 4: 機器人形體 — 給 Node 們一個身體
> 描述機器人物理結構(關節、感測器位置),為 SLAM/Nav2/MoveIt 鋪路。

| Phase | 主題 | 學到什麼 | 🐍 Py |
|-------|------|----------|------|
| [15](phase-15-urdf/) | URDF + robot_state_publisher | xacro 巨集、joint 類型、TF tree 自動生成、tf2_echo CLI 驗證 | — |
| [16](phase-16-tf2/) | TF2 進階 | static/dynamic broadcaster、Listener + Buffer、lookupTransform、業界口訣 | — |
| [17](phase-17-gazebo/) | Gazebo 整合(headless) | gazebo.launch.py vs gzserver、URDF vs SDF、spawn_entity 雷、use_sim_time 必設 | — |
| [18](phase-18-ros2-control/) | ros2_control | URDF 內 hardware 宣告、controller_manager + mock_components、command/state 流動 | — |
| [19](phase-19-pluginlib/) | pluginlib | runtime 載入 C++ class、三套件分離(base/plugins/demo)、Nav2 擴充基礎 | — |
| [20](phase-20-multi-machine/) | 多機通訊 | ROS_DOMAIN_ID 隔離、FastDDS Discovery Server 取代 multicast、docker compose 模擬多機 | — |

### 🚀 Part 5: 領域應用 — 分流深入

> 從這裡分成兩條 Track:**🅰️ Mobile Robot(移動底盤)** vs **🅱️ Arm(機械手臂)**。可擇一深入或兩條都走。

#### 🅰️ Track A:移動底盤(SLAM + Nav2)
> 業界對應:掃地機器人、AGV、配送機器人、自駕車(部分)

| Phase | 主題 | 學到什麼 | 🐍 Py |
|-------|------|----------|------|
| [20A](phase-20A-odometry-ekf/) | Odometry + robot_localization (EKF) | nav_msgs/Odometry 與 IMU covariance、EKF YAML 設定、實測 EKF 勝過 wheel-only 8 倍 | — |
| [21A](phase-21A-slam-toolbox/) | SLAM with slam_toolbox | online async mapping、map→odom TF、WSL GPU 不足造成 queue full 雷的完整解析 | — |
| [22A](phase-22A-nav2-basics/) | Nav2 入門 | 8 個 lifecycle node 全套、planner/controller plugin、base_footprint 雷、/initialpose 必下 | — |
| [23A](phase-23A-nav2-bt-plugin/) | 自訂 Nav2 BT plugin | BT.cpp ConditionNode、BT_REGISTER_NODES 巨集、blackboard 共享 node、4 個 gtest 全過 | — |
| [30](phase-30-nav2-bt-advanced/) | Nav2 BT 進階(4 種 node + 整合)| StatefulActionNode、DecoratorNode、OutputPort、6 個 gtest case、完整充電/巡邏 BT XML | — |
| [🎯 Capstone A](phase-CapstoneA-mobile/) | **Mobile Robot 整合** | Gazebo + Nav2 + 自訂 BT plugin + Action client 自動 waypoint sequence 整合 | — |

#### 🅱️ Track B:機械手臂(MoveIt 2)
> 業界對應:協作手臂(UR、TM)、工業手臂、人形機器人手部

| Phase | 主題 | 學到什麼 | 🐍 Py |
|-------|------|----------|------|
| [20B](phase-20B-arm-urdf/) | 手臂 URDF (xacro + SRDF) | xacro macro 抽 6 個 link 模板、SRDF 為 MoveIt 鋪路、ParameterValue str 雷 | — |
| [22B](phase-22B-moveit-cpp/) | MoveIt 2 C++ API | MoveGroupInterface plan + 4 種 target、3 份 description params、IK 失敗雷 | — |
| 21B | (待完成,需 Setup Assistant GUI) | — | — |
| 23B | (待完成,需視覺驗證) | — | — |
| 🎯 Capstone B | (待完成,需視覺驗證) | — | — |

### 📦 Part 6: 生產化部署 — 上線
> 不論做 mobile 還是 arm,要把作品交付都需要這些。

| Phase | 主題 | 學到什麼 | 🐍 Py |
|-------|------|----------|------|
| [24](phase-24-docker/) | Docker 化 Capstone 1 | multi-stage build、entrypoint.sh exec、`network_mode: host` + `ipc: shareable` 兩大 DDS 雷 | — |
| [25](phase-25-ci-cd/) | CI/CD with GitHub Actions | container build + colcon test + lint matrix + image push 到 GHCR | — |
| [26](phase-26-dds-qos/) | DDS QoS 調校 | Reliability/Durability/Deadline、Reliable↔BestEffort 兼容方向、實測掉包 | — |
| [32](phase-32-rosbag2-advanced/) | rosbag2 進階 | 選擇性錄製、MCAP backend、QoS override 重播、bag 餵回 SLAM、Python 解 bag 出 CSV | ✅ |
| [36](phase-36-diagnostics-watchdog/) | Diagnostics + Heartbeat Watchdog | `diagnostic_updater` + `aggregator`、4 等級 status、library + main + 4 個 gtest case | — |
| [35](phase-35-foxglove-bridge/) | Foxglove Bridge — 即時可視化 | `foxglove_bridge` 一行裝、layout JSON 版控、串 Phase 36 的儀表板 demo | — |

### 🏁 Capstone Final — 整 repo 最終整合
> 整 repo 的 deliverable,**任何有 Docker 的機器都能跑**

| Phase | 主題 | 學到什麼 | 🐍 Py |
|-------|------|----------|------|
| [🎯 Capstone Final](phase-Capstone-Final/) | **Docker 化全套 Mobile Robot** | multi-stage build、host network、ipc shareable、可 push GHCR、實機部署模板 | — |

> 🐍 欄位:✅ 有 rclpy 對照版(資料夾內 `python/`) ｜ — 純觀念或暫無對照

完整學習路徑與 Track 選擇建議見 [ROADMAP.md](ROADMAP.md)。

---

## 📁 每個 phase 的結構

```
phase-XX-topic/
├── README.md              ← 該章完整說明(觀念 + 步驟 + 程式碼)
└── code/
    └── my_cpp_pkg/        ← 完整可編譯的 ROS 2 套件
        ├── package.xml
        ├── CMakeLists.txt
        └── src/*.cpp
```

每章的 `code/my_cpp_pkg/` **都是完整獨立的套件**,不依賴前一章的 code。這意味著:
- 你可以從任何一章開始學
- 但代價是 `CMakeLists.txt` 與 `package.xml` 在各章間會重複(這是刻意的)
- **唯一例外**:`my_robot_interfaces`(Phase 08 定義,Phase 13/14 重用)、`my_arm_description`(Phase 20B 定義,Phase 22B 重用)等 description package

---

## 🛠️ 怎麼跑這些 code

本路徑支援兩種環境,**程式碼完全通用**,差異只在執行時的 topic remapping:

- ☁️ **TheConstructSim 雲端**(免裝即用,前期推薦)
- 💻 **本機 WSL2 / Ubuntu 22.04 + ROS 2 Humble**(後期專案推薦)

完整環境設定步驟、兩者比較表、各章建議的環境,見 **[SETUP.md](SETUP.md)**。

快速版:
```bash
# 兩種環境通用
cd ~/ros2_ws
colcon build --packages-select my_cpp_pkg
source install/setup.bash
ros2 run my_cpp_pkg <executable> --ros-args -r cmd_vel:=<實際topic>
```

或者直接用 Docker 跑 Capstone Final:
```bash
docker compose -f phase-Capstone-Final/code/docker-capstone-a/docker-compose.yml up
```

---

## 📦 舊筆記

`_archive/` 裡是重整前的原始 markdown,保留比對用,不再維護。

---

## 🤖 給未來 AI 協作者

接手這個 repo 的 AI session 請按順序讀:

1. **[`HANDOFF.md`](HANDOFF.md)** — 完整接手指引:使用者背景、進度地圖、踩過的雷、溝通眉角、WSL 工具鏈
2. **[`AUTHORING_GUIDE.md`](AUTHORING_GUIDE.md)** — 章節寫作模板:資料夾結構、README 骨架、程式碼風格、反模式清單
3. **[`PORTFOLIO.md`](PORTFOLIO.md)** — 履歷友善版,看 repo 整體 highlights

讀完這三份你就能無縫接續現有節奏、不用重做已完成的工作。
