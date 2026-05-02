# ROS 2 學習路徑（修正版）

這是 [Gemini 整理的原版路徑](_archive/) 經過審視後的修正版本。主要改動：補上多執行緒/生命週期、提前 URDF、補 Gazebo 整合、補 Odometry 融合、提前 Docker。

> ✅ 已完成 ｜ 🚧 進行中 ｜ ⬜ 未開始

---

## 第一階段：ROS 2 核心通訊

| Phase | 主題 | 狀態 |
|-------|------|------|
| 01 | 雲端環境 + Publisher（cmd_vel 控制） | ✅ |
| 02 | 通訊機制核心觀念（Node/Topic/Msg） | ✅ |
| 03 | Subscriber + 光達避障（QoS, PointCloud2） | ✅ |
| 04 | Services（SetBool 開關） | ✅ |
| 05 | Parameters（外部動態調參，免重編） | ⬜ |
| 06 | Custom Interfaces（自訂 .msg / .srv） | ⬜ |
| **6.5** | **Executors / Callback Groups / LifecycleNode**（新增） | ⬜ |

> **為什麼新增 6.5**：Phase 04 已經出現 service callback 和 sub callback 共享狀態的問題（race condition）。進到 Nav2 / ros2_control 之前必須先理解多執行緒 callback 與生命週期節點。

---

## 第二階段：系統整合

| Phase | 主題 | 狀態 |
|-------|------|------|
| 07 | Launch Files（一鍵啟動多節點） | ⬜ |
| **7.5** | **Gazebo 整合（spawn URDF, gazebo_ros plugins）**（新增） | ⬜ |
| 08 | Actions（長時間任務 + Feedback + Cancel） | ⬜ |
| **09** | **URDF + robot_state_publisher**（從第三階段提前） | ⬜ |
| 10 | TF2 座標轉換樹 | ⬜ |

> **為什麼把 URDF 提前**：TF2 樹 99% 是 URDF 經由 `robot_state_publisher` 自動發出來的。先學 URDF 再學 TF2，才能對應到實務。原路徑把 URDF 排在 TF2 之後，會讓你一直手刻 `static_transform_publisher`。
>
> **為什麼新增 7.5**：本機要練 SLAM/Nav2 必須會自己起 Gazebo + spawn 機器人，TheConstruct 雲端把這塊隱藏掉了。

---

## 第三階段：機器人學應用

| Phase | 主題 | 狀態 |
|-------|------|------|
| 11 | SLAM（同步建圖與定位） | ⬜ |
| **11.5** | **Odometry + robot_localization (EKF)**（新增） | ⬜ |
| 12 | Nav2（自動導航框架） | ⬜ |

> **為什麼新增 11.5**：直接從 SLAM 跳 Nav2 會在 EKF 融合 IMU+odom 卡住。

---

## 第四階段：企業級部署

| Phase | 主題 | 狀態 |
|-------|------|------|
| **13** | **Docker + Dev Container**（提前） | ⬜ |
| 14 | ros2_control（硬體抽象層） | ⬜ |
| 15 | 多機通訊（ROS_DOMAIN_ID, FastDDS Discovery Server） | ⬜ |
| 16 | DDS QoS 調校 | ⬜ |

> **為什麼把 Docker 提前**：很多 ROS 2 開發者一開始就在 container 裡寫，省去裝環境的痛苦。學會 Docker 後續所有實驗都更乾淨。
>
> **為什麼新增 15**：在調 DDS QoS 之前，先學會多機之間的 discovery（DOMAIN_ID 隔離、Discovery Server）更實用。

---

## 與原路徑的差異總結

| 改動 | 為什麼 |
|------|--------|
| ➕ Phase 6.5：Executors/Lifecycle | Phase 04 已踩到多執行緒 race，進階前必補 |
| 🔀 URDF 從第三階段提前到 Phase 09 | TF2 依賴 URDF，順序顛倒 |
| ➕ Phase 7.5：Gazebo 整合 | 本機練 SLAM/Nav2 必備 |
| ➕ Phase 11.5：Odometry + EKF | SLAM → Nav2 之間的橋樑 |
| 🔀 Docker 提前到 Phase 13（第四階段第一） | 早用 container 早輕鬆 |
| ➕ Phase 15：多機通訊 | 比 DDS QoS 調校更早會用到 |
