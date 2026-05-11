# 🧍 Humanoid — 人形機器人

> 歡迎來到機器人開發的「終極深水區」。在這裡，我們將捨棄傳統的輪式底盤與 Gazebo 模擬器，進入高複雜度的全身控制 (Whole-Body Control) 與 MuJoCo 高效能動態模擬世界。

**狀態**:🟡 進行中 — 01 / 02 / 03 已寫文字草稿(⏸,等實際驗證)

---

## 🎯 學完整條支線你會

- 使用專為複雜接觸與動態模擬設計的 **MuJoCo** 模擬器，並將其橋接至 ROS 2。
- 召喚一隻完整的人形機器人 (例如開源的 Unitree G1)。
- 了解高自由度 (>20 DOF) 機器人的關節群組管理，並使用 `ros2_control` 送出全身關節控制指令。
- 探索逆向運動學 (IK) 在雙足機器人上的應用（維持質心平衡）。

---

## 🏭 業界對應

| 平台 | 公司 | 應用場景 |
|------|------|-----------|
| **Optimus** | Tesla | 工廠自動化、搬運、通用勞動 |
| **Figure 01/02** | Figure AI | 汽車產線組裝 (與 BMW 合作) |
| **G1 / H1** | Unitree (宇樹) | 科研教育、輕量級工業應用 |
| **Digit** | Agility Robotics | 倉儲物流搬運 (與 Amazon 合作) |

**業界趨勢 2024+**: 人形機器人是目前資本與研究最密集的賽道。它的難點不再是「導航」，而在於「動態平衡」與「靈巧操作」。這條支線會帶你初窺門徑。

---

## 📋 預計章節結構

```
humanoid/
├── README.md                        ← 你正在讀
├── 01-mujoco-simulation/            ⏸ 文字草稿(已寫)
├── 02-whole-body-control/           ⏸ 文字草稿(已寫)
└── 03-theconstructsim-nao/          ⏸ 文字草稿(已寫)
```

---

## 🧭 章節預告

### 01. MuJoCo Simulation — 高效能動態模擬

**學完你會**:
- 安裝並啟動 `mujoco` 與 `mujoco_ros2_control`。
- 了解為什麼人形機器人捨棄 Gazebo 而擁抱 MuJoCo (接觸動力學的極致)。
- 在 MuJoCo 中載入 Unitree G1 (或其他人形機器人) 的 MJCF 模型。
- 啟動 ROS 2 的 `joint_state_broadcaster` 與 `forward_command_controller`，透過 ROS 2 Topic 控制人形機器人的關節。

**核心套件**:
- `mujoco` (Google DeepMind 開源物理引擎)
- `mujoco_ros2_control` (將 MuJoCo 與 ros2_control 橋接)

**為什麼重要**: **MuJoCo 是目前最主流的強化學習 (RL) 與雙足機器人模擬器**。

**預估時長**: 1 day
**環境**: 💻 本機 Ubuntu / WSL2 (需圖形化介面，雲端模擬器不支援 MuJoCo 的 Native 渲染)

---

### 02. Whole-Body Control (WBC) — 全身控制基礎

**學完你會**:
- 寫一個 C++ / Python Node，同時控制人形機器人的雙臂與雙腿。
- 了解如何用 Pinocchio 這種剛體動力學函式庫計算重心 (CoM)。
- 讓機器人在原地做「下蹲」、「揮手」等動作，並透過控制維持不跌倒。

**整合主線**:
- Phase 18 ros2_control
- Phase 22B MoveIt C++ API (可用 MoveIt 幫雙手解 IK)

**預估時長**: 2 days
**環境**: 💻 本機

---

## 📦 環境需求 (本地)

```bash
# 安裝 MuJoCo
sudo apt update
sudo apt install libglfw3-dev
# 下載並解壓縮 MuJoCo (官方預先編譯版本)
# 或是直接安裝 ros-humble-mujoco 相關套件（若社群已打包）

# 安裝 mujoco_ros2_control 相關依賴
sudo apt install ros-humble-ros2-control ros-humble-ros2-controllers
```

---

## 🚦 開始之前

確認主線進度 (至少要做完):
- ✅ Phase 18 (ros2_control) — 人形機器人關節控制的命脈，沒看懂會完全不知所措。
- ✅ Phase 20B (URDF) — 雖然 MuJoCo 使用 MJCF 格式，但 ROS 2 還是需要 URDF 來發布 TF 樹。
- ✅ Phase 22B (MoveIt) — 準備給手臂做動作規劃。

---

## ⏭️ 從哪開始

主線完成後，進入 **[01-mujoco-simulation](01-mujoco-simulation/)**，把機器人叫出來！
