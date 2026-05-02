# ROS 2 學習路徑

> **適用範圍**：本路徑同時涵蓋**移動底盤 (Mobile Robot)** 與**機械手臂 (Manipulator)** 兩大主軸。
> 前段（Phase 01–11）是兩者共用的核心基礎；後段分為 Track A（Mobile）與 Track B（Arm），可擇一深入或兩條都走。

> **執行環境**：每個 phase 的程式碼都能在 **TheConstructSim 雲端** 或 **本機 WSL2** 跑。差異只在執行時的 topic remapping。
> 環境設定、選擇建議與兩種環境的對照表見 **[SETUP.md](SETUP.md)**。

> ✅ 已完成 ｜ 🚧 進行中 ｜ ⬜ 未開始

---

## 整體結構地圖

```
                    ┌─ Track A: Mobile Robot (Phase 14A–17A)
共用核心             │   SLAM, Nav2, Odom 融合
Phase 01–13   ──────┤
通訊 + 系統 + 工具    │
                    └─ Track B: Manipulator (Phase 14B–17B)
                        MoveIt 2, 軌跡規劃, 抓取

           ┌─ 兩條 Track 收斂於 ─┐
           │  Phase 18–21        │
           │  生產化 (Docker, CI │
           │  測試, 多機, DDS)   │
           └─────────────────────┘
```

---

## 共用核心（Phase 01–13）

> 這段不論做 mobile 還是 arm 都必須完整走過。沒有捷徑。

### 第一階段：ROS 2 通訊機制

| Phase | 主題 | 預計時長 | 狀態 |
|-------|------|---------|------|
| 01 | 雲端環境 + Publisher | 2 hr | ✅ |
| 02 | 通訊機制核心觀念 | 1 hr | ✅ |
| 03 | Subscriber + 光達避障（QoS, PointCloud2） | 3 hr | ✅ |
| 04 | Services（SetBool 開關） | 2 hr | ✅ |
| 05 | **Debug 工具集**（rqt_graph / rqt_plot / ros2 bag / rqt_console） | 3 hr | ✅ |
| 06 | Parameters（外部動態調參） | 2 hr | ⬜ |
| 07 | Custom Interfaces（自訂 .msg / .srv / .action） | 3 hr | ⬜ |
| 08 | Executors / Callback Groups / LifecycleNode | 4 hr | ⬜ |
| **🎯 Capstone 1** | **可調參數 + Service 開關 + Custom Msg 的整合節點，丟 GitHub** | 1 day | ⬜ |

> **新增 Phase 05（Debug 工具）**：這是每天都會用、卻沒被獨立教的東西。佔 ROS 開發時間的 50%。
> **新增 Capstone 1**：強迫整合應用，避免學完一堆機制卻組不出系統。

### 第二階段：系統整合

| Phase | 主題 | 預計時長 | 狀態 |
|-------|------|---------|------|
| 09 | Launch Files（Python launch script） | 3 hr | ⬜ |
| 10 | **單元測試與 launch 整合測試**（gtest + launch_testing） | 4 hr | ⬜ |
| 11 | Actions（長時間任務 + Feedback + Cancel） | 3 hr | ⬜ |
| 12 | URDF + robot_state_publisher | 1 day | ⬜ |
| 13 | TF2 座標轉換樹（含時間戳處理、buffer/listener） | 1 day | ⬜ |

> **新增 Phase 10（測試）**：產線級 ROS code 不能沒有測試。提前到第二階段養成習慣。
> **URDF 提前到 Phase 12**：因為 TF2 99% 是 URDF + robot_state_publisher 自動產生的。

---

## 模擬與硬體抽象（Phase 14–15，共用）

| Phase | 主題 | 預計時長 | 狀態 | 業界對應 |
|-------|------|---------|------|---------|
| 14 | Gazebo 整合（spawn URDF, gazebo_ros plugins） | 1 day | ⬜ | 所有 ROS 開發前置 |
| 15 | ros2_control（硬體抽象層 + controller manager） | 2 day | ⬜ | AGV、協作手臂、所有實體機器人 |

> **為什麼這兩章共用**：mobile 和 arm 都要在 Gazebo 裡先模擬、都要透過 ros2_control 控制底層。學完這兩章後再分流。

---

## 🅰️ Track A：移動底盤（Mobile Robot）

> 目標：能在未知環境建圖、自動導航、避開動態障礙物。
> 業界對應：掃地機器人、AGV、配送機器人、自駕車（部分）。

| Phase | 主題 | 預計時長 | 狀態 |
|-------|------|---------|------|
| 16A | Odometry 基礎 + robot_localization (EKF 融合 IMU/odom) | 2 day | ⬜ |
| 17A | SLAM（slam_toolbox 即時建圖） | 2 day | ⬜ |
| 18A | Nav2 入門（Costmap、Planner、Controller、Behavior Tree） | 3–5 day | ⬜ |
| 19A | Nav2 進階（自訂 BT plugin、動態避障、多目標巡邏） | 3 day | ⬜ |
| **🎯 Capstone A** | **本機 Gazebo 起 TurtleBot3 → SLAM 建圖 → Nav2 自動導航到指定點，錄影驗證** | 2 day | ⬜ |

---

## 🅱️ Track B：機械手臂（Manipulator）

> 目標：能規劃手臂運動軌跡、避開障礙物、執行抓取任務。
> 業界對應：協作手臂（UR、TM、AUBO）、工業手臂、人形機器人手部。

| Phase | 主題 | 預計時長 | 狀態 |
|-------|------|---------|------|
| 16B | 手臂 URDF（joint 類型、xacro、SRDF） | 2 day | ⬜ |
| 17B | MoveIt 2 入門（MoveIt Setup Assistant、RViz 操作面板） | 2 day | ⬜ |
| 18B | MoveIt 2 程式控制（C++ MoveGroupInterface、軌跡規劃 API） | 3 day | ⬜ |
| 19B | 進階主題（碰撞檢測、Pick & Place、視覺整合） | 3–5 day | ⬜ |
| **🎯 Capstone B** | **Gazebo 起 UR5/Panda 手臂 → MoveIt 規劃路徑 → 模擬抓取一個方塊，錄影驗證** | 2 day | ⬜ |

> **學完 Track B 你會知道**：MoveIt Setup Assistant 怎麼用、planning group / kinematics solver / OMPL planner 是什麼、PlanningScene 如何加障礙物、`MoveGroupInterface::plan()` vs `execute()` 差異、grasp pose 怎麼算。

---

## 收斂：生產化部署（Phase 20–24，兩條 Track 共用）

> 不論你做 mobile 還是 arm，要把作品上線都需要這些。

| Phase | 主題 | 預計時長 | 狀態 | 業界對應 |
|-------|------|---------|------|---------|
| 20 | Docker + Dev Container（含 ROS 2 image 最佳實踐） | 2 day | ⬜ | 所有公司都用 |
| 21 | CI/CD（GitHub Actions 跑 colcon test、image build） | 1 day | ⬜ | 任何工程團隊 |
| 22 | 多機通訊（ROS_DOMAIN_ID, FastDDS Discovery Server） | 1 day | ⬜ | 車隊、工廠多機 |
| 23 | DDS QoS 調校（Reliability、Durability、Deadline） | 2 day | ⬜ | 高即時性系統 |
| 24 | 部署到實機（Raspberry Pi / Jetson / 工控機） | 2–3 day | ⬜ | 量產前最後一哩 |
| **🎯 Capstone Final** | **把 Capstone A 或 B 完整 docker 化，CI 跑測試，一鍵 `docker compose up` 重現** | 2 day | ⬜ |

---

## 範圍外的事（不在本路徑）

刻意不放進來避免發散：
- **ROS 1 → ROS 2 遷移**：除非你在維護舊系統，否則跳過
- **Gazebo Harmonic / Ignition**：本路徑用 Gazebo Classic 11（與 Humble 最相容）
- **ROS 2 Rolling / Iron / Jazzy**：學完 Humble 後升級就好
- **深度學習整合（YOLO + ROS）**：那是 perception 領域的另一條路徑
- **Behavior Tree 設計理論**：Nav2 章節會用到 BT.CPP，但 BT 設計本身是另一個學科

---

## 兩個 Track 的選擇建議

| 你想做的事 | 建議 Track |
|-----------|-----------|
| 自駕車、AGV、掃地機 | A（Mobile） |
| 協作手臂、工業自動化、Pick & Place | B（Arm） |
| 人形機器人 | **A 和 B 都要** |
| 移動操作（mobile manipulator，車+手臂） | **A 和 B 都要**，最後再學 Phase 25（whole-body control，本路徑暫無） |
| 不確定，想先試試水溫 | 先 A（Nav2 比 MoveIt 文件好懂、社群活躍度高） |

---

## 與原版（Gemini 整理版）的差異總結

| 改動 | 為什麼 |
|------|--------|
| ➕ 完整新增 Track B（機械手臂） | 原路徑只有 mobile，業界手臂市場同樣大 |
| ➕ Phase 05：Debug 工具集 | 每天都用的東西，原路徑沒獨立講 |
| ➕ Phase 10：測試（gtest + launch_testing） | 從第二階段就養成測試習慣，不是企業級才補 |
| ➕ 三個 Capstone 驗收點 | 避免學完一堆機制卻組不出系統 |
| ➕ Phase 21：CI/CD 獨立成章 | 原版混在 Docker 章，分開更清楚 |
| ➕ Phase 24：部署實機 | 量產前最後一哩，原版完全沒提 |
| 🔀 URDF 提前到 Phase 12（共用核心） | TF2 與 mobile/arm 兩條 Track 都依賴 URDF |
| 🔀 ros2_control 提前到 Phase 15（共用） | 兩條 Track 都需要硬體抽象 |
| 🔀 Docker 提前到 Phase 20（生產化開頭） | 早用 container 早輕鬆 |
| 🔢 編號改線性、無小數 | 之前 6.5 / 11.5 不好維護 |
| ⏱️ 每章加預計時長 | 方便排計畫 |
