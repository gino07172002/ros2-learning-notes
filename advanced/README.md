# 🚀 進階支線(Advanced Tracks)

> 主線 Phase 01–37 涵蓋 ROS 2 通用技能與「**移動底盤(Track A)**」「**機械手臂(Track B)**」兩個垂直領域。
> 這個資料夾收的是**橫切式進階主題** — 不照線性章節走,而是**業界各種特定機器人類別 / 整合主題**。

---

## 🤔 為什麼開新資料夾,不繼續加 Phase?

- 主線 Phase 是**線性學習路徑**,前後有依賴關係(學完 Part 4 才能進 Part 5)
- 這裡的支線是**橫切專題**,做完主線 Part 1–4 後可選讀任何一條
- 各支線獨立可成長,**無人機可寫 10 章不影響別人**
- 命名彈性大(用 `01-` `02-` 而不是 `phase-XX`,沒編號壓力)

---

## 📋 5 條支線

### 🚁 [drone-px4/](drone-px4/) — 無人機(PX4 + ROS 2)

**業界**:DJI、Skydio、自主巡檢、農業無人機、室內 SLAM 無人機

**為什麼學**:PX4 + ROS 2 是 2024+ 業界主流(取代 ROS 1 時代的 MAVROS)。純 SITL 模擬就能跑,不用買硬體。

**前置**:主線 Phase 01–14 + Phase 16(TF2)+ Phase 13(Action)
**章節數**:預計 4 章 + Capstone

---

### 👁️ [perception/](perception/) — 視覺感知

**業界**:幾乎所有現代機器人公司都需要(配 mobile / arm / drone)

**為什麼學**:從 LiDAR(Phase 03)擴到 Camera + 3D 物體偵測,是 production 系統的標配。

**前置**:主線 Phase 01–13(會用 Sub + Custom Interface 即可)
**章節數**:預計 4 章

---

### 🐕 [quadruped/](quadruped/) — 四足機器人

**業界**:Boston Dynamics Spot、Unitree Go/B2、宇樹

**為什麼學**:四足是 2024+ 機器人熱門賽道。`champ` 套件提供純模擬通用四足,不用實機就能練。

**前置**:主線 Phase 18(ros2_control)+ Phase 22A(Nav2)
**章節數**:預計 2–3 章

---

### 🤖 [multi-robot/](multi-robot/) — 多機器人協作

**業界**:倉儲機器人(Amazon、京東)、AGV fleet、無人機群

**為什麼學**:單機 → 多機是 production 系統很常見的擴張路線。

**前置**:主線 Phase 11(namespace)+ Phase 20(多機通訊)+ Phase 22A(Nav2)
**章節數**:預計 2 章

---

### 🧍 [humanoid/](humanoid/) — 人形機器人

**業界**:Tesla Optimus、Figure、Unitree G1/H1、Agility Robotics Digit

**為什麼學**:人形機器人是 2024 年後機器人領域最火熱的終極目標。涉及極度複雜的全身控制 (WBC, Whole-Body Control) 與動態平衡。學會如何在 MuJoCo 中模擬並接入 ROS 2，是進入前沿硬體科技公司的敲門磚。

**前置**:主線 Phase 18(ros2_control)+ Phase 22B(MoveIt C++ API)
**章節數**:預計 2 章

---

## 🗺️ 進度狀態

| 支線 | 狀態 | 章節進度 | 備註 |
|------|------|--------|------|
| 🚁 drone-px4 | 🟡 進行中 | **1 / 4 文字草稿** | 01 px4-bridge ⏸ 已寫 |
| 👁️ perception | 🟡 進行中 | **4 / 4 文字草稿** | 01 cv_bridge / 02 AprilTag / 03 YOLO / 04 PCL ⏸ 已寫 |
| 🐕 quadruped | 🟡 進行中 | **1 / 3 文字草稿** | 01 champ-simulation ⏸ 已寫 |
| 🧍 humanoid | 🟡 進行中 | **2 / 2 文字草稿** | 01 mujoco-simulation / 02 whole-body-control ⏸ 已寫 |
| 🤖 multi-robot | 🟡 進行中 | **2 / 2 文字草稿** | 01 namespace-spawn / 02 fleet ⏸ 已寫 |

> **「⏸ 文字草稿」**:README + code 骨架已寫,**沒在實機 / 雲端跑過驗證**。雷區清單從業界經驗整理,實做時可能會修細節 / 踩到沒寫的雷。等 gino 跑過後升 ✅。

---

## 📐 支線寫作慣例

跟主線 Phase 一樣的規則,**有兩個差異**:

| 規則 | 主線 | 支線 |
|------|------|------|
| 命名 | `phase-XX-topic` | `<track>/01-topic` `<track>/02-topic` |
| 編號 | 全 repo 全域編號 | 每條支線本地編號 |
| 「驗證過」原則 | 必須真實在 WSL/雲端跑出來 | 同 |
| 雙環境支援 | ☁️💻 並列 | 同 |
| 雷區紀錄 | 5–8 條 | 同 |

詳見 [AUTHORING_GUIDE.md](../AUTHORING_GUIDE.md)。

---

## 🤝 想做哪條?

挑一條最感興趣的:
- 想做飛的:🚁 drone-px4
- 想做視覺的:👁️ perception
- 想做四足的:🐕 quadruped
- 想做人形的:🧍 humanoid
- 想做多機協調的:🤖 multi-robot

點進去看該支線的詳細規劃,或回主線 [README.md](../README.md) 繼續走線性教程。
