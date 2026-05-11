# 🐕 Quadruped — 四足機器人

> 用 `champ` 通用四足模擬器跑 ROS 2 stack。Boston Dynamics Spot、Unitree Go 都用同樣的 ros2_control + Nav2 模式。

**狀態**:🟡 進行中 — 01 已寫文字草稿(⏸,等實際驗證)

---

## 🎯 學完整條支線你會

- 在 Gazebo 跑 `champ`(通用四足模擬器,12 DOF)
- 控制四足機器人 trotting / walking 步態
- 把 Phase 22A Nav2 套用到四足(以 cmd_vel 為介面,Nav2 不知道是輪子還是腳)
- 知道 Spot / Unitree 等實機的 SDK 怎麼接 ROS 2

---

## 🏭 業界對應

| 平台 | 公司 | ROS 2 整合 |
|------|------|-----------|
| **Spot** | Boston Dynamics | [`spot_ros2`](https://github.com/bdaiinstitute/spot_ros2)(社群) |
| **Go2 / B2** | Unitree | [`unitree_ros2`](https://github.com/unitreerobotics/unitree_ros2) |
| **A1 / Mini** | Unitree(舊款) | `champ` 範本可改 |
| **ANYmal** | ANYbotics | 自家 SDK + ROS bridge |

**業界趨勢 2024+**:四足機器人從研究品變成**真實量產設備**(物業巡檢、倉儲、軍用)。會 ros2_control + Nav2 + 四足模擬就能進這條賽道。

---

## 📋 預計章節結構

```
quadruped/
├── README.md                        ← 你正在讀
├── 01-champ-simulation/             ⏸ 文字草稿(已寫)
├── 02-gait-control/                 ← Trotting / Walking 步態切換
└── 03-nav2-on-quadruped/            ← Nav2 套用在四足上(cmd_vel 介面通用)
```

---

## 🧭 章節預告

### 01. CHAMP Simulation — 通用四足模擬

**學完你會**:
- 安裝並啟動 `champ`(`champ_description` + `champ_navigation`)
- 在 Gazebo 看到一隻 12 DOF 四足機器人 spawn
- 用 `teleop_twist_keyboard` 鍵盤控制走、轉、原地踏步
- 看 `joint_states` topic 發了哪些 joint 位置

**核心套件**:
- `champ`(MIT-licensed 通用四足 stack)
- `champ_description`、`champ_navigation`、`champ_teleop`

**為什麼重要**:**champ 是學四足最便宜的方式**(0 元 + 純模擬)。同樣架構可移植到 Spot / Unitree 真機。

**預估時長**:1 day(主要在裝套件 + 確認 Gazebo 跑得順)
**環境**:💻 本機(Gazebo + champ 雲端較少預裝)

---

### 02. Gait Control — 步態控制

**學完你會**:
- 看穿四足的「**OpenLoop gait controller**」內部運作(對比輪子的 diff_drive_controller)
- 切換 gait pattern(trot / pace / bound)看效果差異
- 調 gait 參數(`stance_duration` / `stance_depth` / `nominal_height`)
- 寫 ROS Node 動態切 gait(用 ROS Parameters 或自訂 .srv)

**整合主線**:
- Phase 18 ros2_control(四足內部用 4 個 leg controller)
- Phase 06 Parameters(動態調 gait 參數)
- Phase 19 pluginlib(進階:寫自訂 gait controller)

**預估時長**:1 day
**環境**:💻 本機

---

### 03. Nav2 on Quadruped — Nav2 套用四足

**學完你會**:
- 把 [Phase 22A Nav2](../../phase-22A-nav2-basics/) 直接套用到 champ(超神奇 — `cmd_vel` 介面通用)
- 設 robot_base_frame 為 `base` / `body`(四足慣例,不是 turtlebot 的 base_footprint)
- 看穿「四足 odometry 比輪子更不準」的雷 — IMU 融合更重要
- 整合 SLAM(Phase 21A)在四足上建圖

**為什麼這章值得做**:
- 證明 **Nav2 是「介面之上」的 stack** — 跟底盤類型無關
- 業界 Spot 跑 Nav2 就是這樣做的(Spot 自家 SDK 上層套 Nav2)

**整合主線**:
- Phase 21A SLAM + Phase 22A Nav2 全套
- Phase 20A EKF(IMU 對四足比輪式更重要)

**預估時長**:1 day
**環境**:💻 本機(SLAM/Nav2 在 WSL 沒 GPU 結構驗證可,真實 demo 推薦雲端)

---

## 📦 環境需求(本地)

```bash
# 主線 ROS 2 環境已就緒,額外:

# 1. champ 套件(從 source build)
cd ~/ros2_ws/src
git clone --recursive https://github.com/chvmp/champ.git -b ros2
git clone https://github.com/chvmp/champ_teleop.git -b ros2

# 2. rosdep 安裝相依
cd ~/ros2_ws
rosdep install --from-paths src --ignore-src -r -y

# 3. build
colcon build --packages-select champ_description champ_navigation \
                                champ_msgs champ_teleop
```

> ⚠️ **champ 的 ROS 2 port 有點久沒維護**(主分支還是 ROS 1)。寫章節時要驗證 Humble 相容性。

---

## 🐛 預期會踩的雷

1. **champ ROS 2 分支 build 失敗** — 上游主分支是 ROS 1,要確認 fork / branch 對(寫章節時找到能用的 commit)
2. **Gazebo 內四足腳掉到地下** — physics ground friction 沒設,leg slip;`<gazebo>` 加 `mu1`/`mu2` 參數
3. **走起來會抖、會翻車** — gait timing 跟模擬步長(physics step)不對齊
4. **odometry 飄超快** — 四足比輪子 odometry 差很多,**必須跟 IMU 融合(Phase 20A)**
5. **Nav2 local_costmap 跟 robot_base_frame 不對齊** — 四足慣例叫 `base`,不是 `base_footprint`,要改 nav2_params.yaml

---

## 🔗 學習資源

- [CHAMP GitHub](https://github.com/chvmp/champ)
- [Spot ROS 2(BDAI Institute)](https://github.com/bdaiinstitute/spot_ros2)
- [Unitree ROS 2](https://github.com/unitreerobotics/unitree_ros2)

---

## 🚦 開始之前

確認主線進度(至少要做完):
- ✅ Phase 18(ros2_control) — 第 02 章必備
- ✅ Phase 22A(Nav2 入門) — 第 03 章必備
- 推薦 ✅ Phase 17(Gazebo)
- 推薦 ✅ Phase 20A(EKF) — 第 03 章 odometry 融合用

---

## ⏭️ 從哪開始

主線完成後,第一章會是 **01-champ-simulation**(待寫):CHAMP + Gazebo 模擬通用四足。

> 這條支線目前只有 README 骨架。實際章節會在 gino 開始做時逐章補。
