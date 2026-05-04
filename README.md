# ROS 2 學習筆記（C++）

從零開始學 ROS 2 Humble 的實戰筆記。每個 phase 是獨立的專案資料夾,自帶完整可編譯的 code 與說明,**可跳讀、可單獨執行**。

---

## 👋 你是誰?從這裡開始

| 你的情況 | 該看哪份 | 一句話說明 |
|---------|---------|-----------|
| 🆕 **沒碰過 ROS 2** | **[GETTING_STARTED.md](GETTING_STARTED.md)** | 用 MQTT/gRPC 對照解釋 ROS 2 是什麼,給你一條 7 章新手路徑(約 1 週下午時段),跑完就會寫多節點通訊系統 |
| 🚀 **會 ROS 1 想學 ROS 2** | 仍從 [Phase 01](phase-01-cloud-env-first-publisher/) 起 | 觀念差很多,Phase 02 會說清楚為什麼 ROS 2 全部重寫;之後可跳到 Phase 09(Executor / Lifecycle / Composition — ROS 2 才有的) |
| 🏛️ **想學 library 設計觀** | **[DESIGN_NOTES.md](DESIGN_NOTES.md)** | 把 ROS 2 當成 library 設計教材讀,深挖 init/spin/shutdown / Executor / shared_ptr 約束等設計決策,提煉可帶走的設計通則 |
| 🚀 **想做進階機器人**(無人機 / 視覺 / 四足 / 多機) | **[advanced/](advanced/)** | 主線之外的 4 條橫切支線:🚁 drone-px4 / 👁️ perception / 🐕 quadruped / 🤖 multi-robot,做完 Part 4 即可選讀 |
| 💼 **招聘方 / Code reviewer** | **[PORTFOLIO.md](PORTFOLIO.md)** | 30 秒看完技術棧、3 個最強章節、6 條最有故事的雷 |
| 🗺️ **想看完整學習地圖** | [ROADMAP.md](ROADMAP.md) | Part 1–6 結構、Track A(Mobile)/ B(Arm)分流、各章預計時長 |
| ⚙️ **要設定環境** | [SETUP.md](SETUP.md) | TheConstructSim ☁️ vs 本機 WSL2 💻 兩種環境完整步驟 + 比較表 |

> 下面是 repo 全章節地圖,**新手不用一次讀完**。先去 [GETTING_STARTED.md](GETTING_STARTED.md)。

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

## 🧰 進階章節:工程化生態(Phase 30+)

> 給已經會基本 ROS 2 (Phase 01–26) 的人。這 5 章把「**寫 production-ready ROS 2 系統**」的關鍵套件全部跑過一遍 — Behavior Tree、可觀測性、可視化、Lifecycle 健康監控、離線分析。每章獨立可學,但接起來就是完整的 production stack。

```
┌── 行為決策 ──┐    ┌── 可觀測性 ──┐    ┌── 可視化 ──┐
│  Phase 30     │    │  Phase 36     │    │  Phase 35   │
│  Nav2 BT 進階 │    │  Watchdog +   │    │  Foxglove   │
│  4 種 BT node │    │  Diagnostics  │    │  Bridge     │
└───────────────┘    └───────┬───────┘    └──────┬──────┘
                             │                    │
                             ▼                    │
                     ┌─────────────────┐          │
                     │  Phase 37       │          │
                     │  Lifecycle +    │          │
                     │  Diagnostics    │ ◄────────┘
                     │  整合骨架        │
                     └─────────────────┘
                             │
                             ▼
                     ┌─────────────────┐
                     │  Phase 32       │
                     │  rosbag2 進階   │
                     │  (錄 + 離線分析) │
                     └─────────────────┘
```

| 章節 | 角色 | 為什麼選它 |
|------|------|-----------|
| [Phase 30 Nav2 BT 進階](phase-30-nav2-bt-advanced/) | **行為決策** — 4 種 BT node 完整 plugin 集 | StatefulActionNode 三態、DecoratorNode RUNNING 傳遞、OutputPort 寫 blackboard,**Nav2 客製化的核心技能**;6 個 gtest case |
| [Phase 36 Diagnostics + Watchdog](phase-36-diagnostics-watchdog/) | **可觀測性** — 健康監控的事實標準 | `diagnostic_updater::Updater` + `aggregator`、library + main + 4 個 gtest;業界 oncall paging 都靠 `/diagnostics_agg` |
| [Phase 35 Foxglove Bridge](phase-35-foxglove-bridge/) | **可視化** — RViz 看不到的東西全在這 | `apt install` 一行、layout JSON 版控、串 Phase 36 demo 一鍵起儀表板;**2024 後新專案 90% 用 Foxglove** |
| [Phase 37 Lifecycle + Diagnostics 整合](phase-37-lifecycle-diagnostics/) | **production node 骨架** | LifecycleNode 每個 transition 自動發 diagnostic、`MultiThreadedExecutor` 解 service 死鎖;5 個 gtest case;**抄這個就能寫實機 node** |
| [Phase 32 rosbag2 進階](phase-32-rosbag2-advanced/) | **離線分析 pipeline** | 選擇性錄製、MCAP backend、QoS override 重播、bag → SLAM 離線建圖、`rosbag2_py` 解 bag 出 CSV |

**完整 production demo**(一條 launch 啟全部):

```bash
# Phase 36 watchdog + Phase 35 Foxglove 串起來
ros2 launch my_foxglove_demo diagnostics_with_bridge.launch.py
# 瀏覽器:https://app.foxglove.dev → ws://localhost:8765 → import diagnostics_layout.json
```

---

## 🗺️ 完整章節結構

> 章節依「**學習層次**」分成幾個 Part。每個 Part 內各章可跳讀,但 Part 之間有遞進關係 — 下一個 Part 假設你能用前一個 Part 的東西。

### 📖 Part 1: 通訊基礎 — 讓 Node 能講話
> 學完 Part 1 你能用現成的 ROS 訊息類型寫出 Pub/Sub/Service Node。

| Phase | 主題 | 為什麼要學這章 | 學到什麼 | 🐍 Py |
|-------|------|---------------|----------|------|
| [01](phase-01-cloud-env-first-publisher/) | 雲端環境 + 第一支 Publisher | 「ROS 2 是什麼?」用最簡 demo 親手驗證 | 建 ROS 2 套件、用 colcon 編譯、寫 Publisher 讓車子前進 | ✅ |
| [02](phase-02-communication-concepts/) | **ROS 2 設計哲學** | Phase 01 跑過了但「為什麼是這樣」?這章拆給你看 | DDS / 節點發現 / 訊息序列化 / 與 MQTT/gRPC/ROS 1 對比 | — |
| [03](phase-03-subscriber-lidar-brake/) | Subscriber + 光達避障 | 只會發訊息不夠,會收訊息才能做反應式控制 | 寫 Subscriber、QoS(SensorDataQoS)、解析 PointCloud2、做避障邏輯 | ✅ |
| [04](phase-04-services-toggle/) | Service Server + 開關 | Pub/Sub 不能一問一答,「打開避障」這種一次性指令要 Service | Service vs Topic、實作 SetBool 服務、雙終端機驗證 | — |

### 🔧 Part 2: 工具與治理 — 看清楚系統 + 微調系統
> 學完 Part 2 你能 debug 不熟的 ROS 系統、調整 Node 行為而不改 code,且能組合既有元件做小作品。

| Phase | 主題 | 為什麼要學這章 | 學到什麼 | 🐍 Py |
|-------|------|---------------|----------|------|
| [05](phase-05-debug-tools/) | Debug 工具集 | ROS 系統有幾十個節點,光看 code 找不到 bug,佔開發時間 50% | rqt_graph 看通訊圖、ros2 bag 錄製/重播、rqt_plot 即時繪圖、rqt_console 集中 log | — |
| [06](phase-06-parameters/) | Parameters(參數系統) | 「改一個閾值就重編譯」太慢,實戰中你會 100% 用 param 取代 magic number | declare/get/on_set callback、YAML 設定檔、rqt_reconfigure GUI 即時調參 | — |
| [🎯 07](phase-07-mini-capstone-1/) | **Mini Capstone 1**:智能煞車車 | 學完 6 個機制可能組不起來;一個下午整合 Param + Service + LiDAR 做出第一個小作品 | 寫一個 Node 同時當 5 個角色、launch file 入門 | — |

### 🏗️ Part 3: 系統設計 — 自己定義協議與架構
> **核心分水嶺**:從「使用現成 ROS 元件」進到「設計自己的 ROS 系統」。學完 Part 3 你能組起一個多節點專案。

| Phase | 主題 | 為什麼要學這章 | 學到什麼 | 🐍 Py |
|-------|------|---------------|----------|------|
| [08](phase-08-custom-interfaces/) | **Custom Interfaces**(自訂訊息) | 內建訊息(Twist/Imu)不夠用,自己的專案要自己定義訊息協議 | 定義 .msg / .srv / .action、interface 套件分離、rosidl 生成 C++ class | — |
| [09](phase-09-executors-lifecycle-composition/) | Executors / Lifecycle / Composition | 多 callback 為什麼會卡?Node 啟動順序怎麼控?Nav2/MoveIt 都靠這層 | Single vs Multi Executor、CallbackGroup、Lifecycle 五狀態、rclcpp_components | — |
| [10](phase-10-launch-files-basics/) | Launch Files 基礎 | 一次起 5 個 Node + 帶參數,手動 ros2 run 不可能,launch file 是標準 | 4 個漸進範例:最小、remap+param、YAML、CLI args | — |
| [11](phase-11-launch-files-advanced/) | Launch Files 進階 | 多機器人 namespace、Node A 起完才起 B、條件啟動 — 真實專案會用到 | IncludeLaunchDescription、event_handler、條件啟動、namespace 多機器人 | — |
| [12](phase-12-testing/) | 測試(gtest + rclcpp) | 沒測試的 ROS code 不能交付,gtest 是 ROS 2 標準,從 Part 3 養成習慣 | 純邏輯單元測試 + rclcpp 整合測試、colcon test、XML 報告 | — |
| [13](phase-13-actions-advanced/) | Actions 進階 | 「導航 30 秒」這種長任務 Topic/Service 都不夠用,Action 才能進度 + 取消 | reject/abort/cancel 全套處理、SIGINT 觸發 cancel、Countdown demo | — |
| [🎯 Capstone 1](phase-14-capstone-1/) | **ApproachController** | Part 3 的總整合,做一個 GitHub portfolio 等級的多角色 LifecycleNode | 6 角色 LifecycleNode + 自動化 launch + 5 個單元測試(GitHub portfolio-ready) | — |

### 🤖 Part 4: 機器人形體 — 給 Node 們一個身體
> 描述機器人物理結構(關節、感測器位置),為 SLAM/Nav2/MoveIt 鋪路。

| Phase | 主題 | 為什麼要學這章 | 學到什麼 | 🐍 Py |
|-------|------|---------------|----------|------|
| [15](phase-15-urdf/) | URDF + robot_state_publisher | 之前的機器人只是「邏輯概念」,URDF 才能讓系統知道光達在哪、輪距多寬 | xacro 巨集、joint 類型、TF tree 自動生成、tf2_echo CLI 驗證 | — |
| [16](phase-16-tf2/) | TF2 進階 | 「光達看到 5 公尺前有牆」要轉成「機器人前 5 公尺」靠 TF;SLAM/Nav2 全靠這層 | static/dynamic broadcaster、Listener + Buffer、lookupTransform、業界口訣 | — |
| [17](phase-17-gazebo/) | Gazebo 整合 | 沒實機怎麼開發?Gazebo 模擬環境讓你寫的 code 能撞牆能滾動 | gazebo.launch.py vs gzserver、URDF vs SDF、spawn_entity 雷、use_sim_time 必設 | — |
| [18](phase-18-ros2-control/) | ros2_control | 業界 AGV/協作手臂的標準框架,讓「同一份 code 跑模擬 + 跑實機」零改動 | URDF 內 hardware 宣告、controller_manager + mock_components、command/state 流動 | — |
| [19](phase-19-pluginlib/) | pluginlib | runtime 動態載 C++ class — Nav2 自訂 BT、MoveIt 自訂 planner 全靠它,業界職缺剛需 | runtime 載入 C++ class、三套件分離(base/plugins/demo)、Nav2 擴充基礎 | — |
| [20](phase-20-multi-machine/) | 多機通訊 | 真機常常「筆電開 RViz、機器人跑感知」,得學 DOMAIN 隔離 + Discovery Server | ROS_DOMAIN_ID 隔離、FastDDS Discovery Server 取代 multicast、docker compose 模擬多機 | — |

### 🚀 Part 5: 領域應用 — 分流深入

> 從這裡分成兩條 Track:**🅰️ Mobile Robot(移動底盤)** vs **🅱️ Arm(機械手臂)**。可擇一深入或兩條都走。

#### 🅰️ Track A:移動底盤(SLAM + Nav2)
> 業界對應:掃地機器人、AGV、配送機器人、自駕車(部分)

| Phase | 主題 | 為什麼要學這章 | 學到什麼 | 🐍 Py |
|-------|------|---------------|----------|------|
| [20A](phase-20A-odometry-ekf/) | Odometry + robot_localization (EKF) | SLAM/Nav2 都需要平滑準確的 `/odom`,單一 sensor 都不夠 → EKF 融合是標準 | nav_msgs/Odometry 與 IMU covariance、EKF YAML 設定、實測 EKF 勝過 wheel-only 8 倍 | — |
| [21A](phase-21A-slam-toolbox/) | SLAM with slam_toolbox | 沒地圖就沒 Nav2、沒 Nav2 就沒自動導航;業界 90% 用 slam_toolbox | online async mapping、map→odom TF、WSL GPU 不足造成 queue full 雷的完整解析 | — |
| [22A](phase-22A-nav2-basics/) | Nav2 入門 | Nav2 是 ROS 2 移動機器人事實標準,業界工程師 80% 工作都在跟它打交道 | 8 個 lifecycle node 全套、planner/controller plugin、base_footprint 雷、/initialpose 必下 | — |
| [23A](phase-23A-nav2-bt-plugin/) | 自訂 Nav2 BT plugin | 客製化 Nav2 行為(自訂條件 / 自訂動作)= 從「會用 Nav2」變成「會改 Nav2」 | BT.cpp ConditionNode、BT_REGISTER_NODES 巨集、blackboard 共享 node、4 個 gtest 全過 | — |
| [30](phase-30-nav2-bt-advanced/) | Nav2 BT 進階(4 種 node + 整合)| 真實專案要寫長任務 + decorator + 進階 BT,本章 4 種 node 完整覆蓋 | StatefulActionNode、DecoratorNode、OutputPort、6 個 gtest case、完整充電/巡邏 BT XML | — |
| [🎯 Capstone A](phase-CapstoneA-mobile/) | **Mobile Robot 整合** | Track A 集大成 demo:Gazebo + Nav2 + 自訂 BT + 自動 waypoint sequence | Gazebo + Nav2 + 自訂 BT plugin + Action client 自動 waypoint sequence 整合 | — |

#### 🅱️ Track B:機械手臂(MoveIt 2)
> 業界對應:協作手臂(UR、TM)、工業手臂、人形機器人手部

| Phase | 主題 | 為什麼要學這章 | 學到什麼 | 🐍 Py |
|-------|------|---------------|----------|------|
| [20B](phase-20B-arm-urdf/) | 手臂 URDF (xacro + SRDF) | 跟移動底盤不同,手臂的 link 多 + 需要 SRDF 給 MoveIt 看 collision/group | xacro macro 抽 6 個 link 模板、SRDF 為 MoveIt 鋪路、ParameterValue str 雷 | — |
| [21B](phase-21B-moveit-setup-assistant/) ⏸ | MoveIt Setup Assistant | 業界 100% 用 GUI wizard 自動產 MoveIt config,手寫 4 個 yaml 是教學用 | GUI wizard 自動產 4 個 yaml + SRDF + collision matrix(草稿,截圖待補) | — |
| [22B](phase-22B-moveit-cpp/) | MoveIt 2 C++ API | 業界手臂 100% 用 MoveIt 規劃,寫一行 setNamedTarget 就完成軌跡規劃 | MoveGroupInterface plan + 4 種 target、3 份 description params、IK 失敗雷 | — |
| 23B | (待完成,需視覺驗證) | — | — | — |
| 🎯 Capstone B | (待完成,需視覺驗證) | — | — | — |

### 📦 Part 6: 生產化部署 — 上線
> 不論做 mobile 還是 arm,要把作品交付都需要這些。

| Phase | 主題 | 為什麼要學這章 | 學到什麼 | 🐍 Py |
|-------|------|---------------|----------|------|
| [24](phase-24-docker/) | Docker 化 Capstone 1 | 「在我電腦上能跑」不是交付,Docker 才能丟到任何機器(實機 / 雲端 / GHCR) | multi-stage build、entrypoint.sh exec、`network_mode: host` + `ipc: shareable` 兩大 DDS 雷 | — |
| [25](phase-25-ci-cd/) | CI/CD with GitHub Actions | 沒 CI 的 ROS code 不能交付,業界一定有自動 build + test + image push | container build + colcon test + lint matrix + image push 到 GHCR | — |
| [26](phase-26-dds-qos/) | DDS QoS 調校 | 預設 QoS 不夠用,實機掉包 / sensor 跟不上時才會發現,得提前學 | Reliability/Durability/Deadline、Reliable↔BestEffort 兼容方向、實測掉包 | — |
| [32](phase-32-rosbag2-advanced/) | rosbag2 進階 | 實機現場錄資料、回家放離線分析 / 後處理建圖 — 業界 debug 必備流程 | 選擇性錄製、MCAP backend、QoS override 重播、bag 餵回 SLAM、Python 解 bag 出 CSV | ✅ |
| [36](phase-36-diagnostics-watchdog/) | Diagnostics + Heartbeat Watchdog | 業界 oncall paging 都靠 `/diagnostics_agg`,沒它系統爛了沒人知道 | `diagnostic_updater` + `aggregator`、4 等級 status、library + main + 4 個 gtest case | — |
| [35](phase-35-foxglove-bridge/) | Foxglove Bridge — 即時可視化 | 2024 後新專案 90% 用 Foxglove 取代 RViz,layout 可版控比 RViz 強 | `foxglove_bridge` 一行裝、layout JSON 版控、串 Phase 36 的儀表板 demo | — |
| [37](phase-37-lifecycle-diagnostics/) | LifecycleNode + Diagnostics 整合 | 實機部署的 production node 標準骨架 — 抄這個就能寫實機 node | LifecycleNode + Updater 標準骨架、5 個 gtest case、5 條 lifecycle 雷 | — |

### 🏁 Capstone Final — 整 repo 最終整合
> 整 repo 的 deliverable,**任何有 Docker 的機器都能跑**

| Phase | 主題 | 為什麼要學這章 | 學到什麼 | 🐍 Py |
|-------|------|---------------|----------|------|
| [🎯 Capstone Final](phase-Capstone-Final/) | **Docker 化全套 Mobile Robot** | 整 repo 終點:把 Capstone A 整套打包成 1.26GB image,`docker compose up` 一鍵交付 | multi-stage build、host network、ipc shareable、可 push GHCR、實機部署模板 | — |

> 🐍 欄位:✅ 有 rclpy 對照版(資料夾內 `python/`) ｜ — 純觀念或暫無對照

完整學習路徑與 Track 選擇建議見 [ROADMAP.md](ROADMAP.md)。

---

## 🚀 進階支線(advanced/) — 業界特定機器人類別

主線完成 Part 1–4 後可選讀,4 條**橫切式**進階支線。詳見 [advanced/README.md](advanced/)。

| 支線 | 領域 | 業界對應 | 預計章數 | 狀態 |
|------|------|---------|---------|------|
| [🚁 drone-px4/](advanced/drone-px4/) | 無人機(PX4 + ROS 2) | DJI / Skydio / 農業 / 物流無人機 | 4 章 + Capstone | ⬜ 骨架 |
| [👁️ perception/](advanced/perception/) | 視覺感知(相機 / AprilTag / YOLO / PCL) | 所有現代機器人公司必備 | 4 章 | ⬜ 骨架 |
| [🐕 quadruped/](advanced/quadruped/) | 四足(CHAMP 通用模擬) | Boston Dynamics Spot / Unitree Go | 3 章 | ⬜ 骨架 |
| [🤖 multi-robot/](advanced/multi-robot/) | 多機協作 | Amazon 倉儲 / AGV fleet / 群飛無人機 | 2 章 | ⬜ 骨架 |

> 各支線目前只有 README 骨架(寫了「**預計教什麼、前置、套件清單、預期會踩的雷**」),實際章節等使用者選定方向後逐條填。

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
