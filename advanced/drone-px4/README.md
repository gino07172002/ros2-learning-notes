# 🚁 Drone with PX4 + ROS 2

> 業界主流的「**自主無人機**」開發 stack。PX4 是開源飛控韌體,ROS 2 透過 micro-XRCE-DDS bridge 跟 PX4 講話,可以用 ROS 2 程式控制無人機飛行。

**狀態**:🟡 進行中 — 01 已寫文字草稿(⏸,等實際驗證)

---

## 🎯 學完整條支線你會

- 用 PX4 SITL(Software-In-The-Loop)在 Gazebo 模擬無人機飛行,**不用買實機**
- 寫 ROS 2 Node 透過 PX4 訂閱遙測 + 送 offboard control 命令
- 做 takeoff / waypoint navigation / landing 整套自主任務
- 結合 Track A 的 SLAM 做「室內 SLAM 無人機」(Capstone)
- 知道為什麼業界從 MAVROS(ROS 1)轉到 px4_ros_com(ROS 2)

---

## 🏭 業界對應

| 應用 | 公司 / 產品 |
|------|-----------|
| 消費 / 拍攝 | DJI(自家飛控,但生態相通)、Skydio |
| 工業巡檢 | Skydio Dock for X2、Auterion |
| 農業 | XAG、DJI Agras |
| 物流 | Wing(Alphabet)、Zipline |
| 室內 SLAM | Skydio X2、Boston Dynamics Spot 上掛 drone |
| 學術研究 | 全部跑 PX4 + ROS 2 |

**結論**:會 PX4 + ROS 2 = 能進無人機公司寫上層 autonomy。

---

## 📋 預計章節結構

```
drone-px4/
├── README.md                           ← 你正在讀
├── 01-px4-bridge/                      ⏸ 文字草稿(已寫)
├── 02-offboard-control/                ← 從 ROS 2 送速度命令飛起來
├── 03-mission-action/                  ← 航點任務 + Action server
└── capstone-indoor-slam-drone/         ← 結合 Track A SLAM 的整合 demo
```

---

## 🧭 章節預告

### 01. PX4 Bridge — SITL + micro-XRCE-DDS

**學完你會**:
- 啟動 PX4 SITL(Gazebo Classic + iris quadcopter)
- 設定 micro-XRCE-DDS Agent 讓 PX4 跟 ROS 2 講話
- 用 `ros2 topic list` 看到 PX4 公開的 topics(`/fmu/out/vehicle_local_position` 等)
- 用 `ros2 topic echo` 看遙測資料

**核心套件**:
- `px4_msgs`(PX4 訊息定義)
- `Micro-XRCE-DDS-Agent`(從 apt 或 build from source)
- `px4-autopilot`(SITL 模擬)

**為什麼這章重要**:整套無人機開發的入門關卡。**SITL 跑通才能進後面**。
**預估時長**:1 day(主要在裝環境)
**環境**:☁️ TheConstruct 不確定有 PX4(可能要付費 ROSject) / 💻 本機 WSL2 + Gazebo

---

### 02. Offboard Control — 從 ROS 2 控飛行

**學完你會**:
- 寫 ROS 2 Node 切換 PX4 到 OFFBOARD mode
- 送 `TrajectorySetpoint`(位置 / 速度命令)
- 解鎖 + arm + takeoff(完整起飛流程)
- 看穿「PX4 為什麼一切到 OFFBOARD 又跳回 MANUAL」這個常見雷

**核心 API**:
- 訂閱 `/fmu/out/vehicle_status`
- 發布 `/fmu/in/offboard_control_mode`
- 發布 `/fmu/in/trajectory_setpoint`
- 發布 `/fmu/in/vehicle_command`(arm / disarm)

**為什麼這章重要**:第一次「**用 ROS 2 程式控無人機飛起來**」。
**預估時長**:1 day
**環境**:☁️/💻 SITL 雙環境通用(PX4 SITL 可雲端 / 本機)

---

### 03. Mission with Action — 航點任務

**學完你會**:
- 自訂 `WaypointMission.action` interface
- 寫 Action server 接收航點清單,自主依序飛
- Feedback 回報「目前到第幾個 waypoint、剩多遠」
- 中途 cancel / 失敗時 fallback to RTL(Return To Launch)

**整合主線**:
- 用 [Phase 13 Action 進階](../../phase-13-actions-advanced/) 的 cancel/abort 全套處理
- 用 [Phase 16 TF2](../../phase-16-tf2/) 把世界座標轉成 PX4 NED frame

**預估時長**:2 day
**環境**:☁️/💻

---

### Capstone:室內 SLAM 無人機 🎯

**展示**:
- Gazebo 室內房間 + iris quadcopter 帶下視 LiDAR
- slam_toolbox(Phase 21A)即時建圖
- ROS 2 Action 驅動無人機執行 「**繞房間飛一圈 + 建出 occupancy grid**」

**整合主線**:
- Phase 21A SLAM
- Phase 13 Action
- Phase 16 TF2
- 本支線 01–03

**預估時長**:2 day
**環境**:💻 本機(雲端模擬器可能不夠順)

---

## 📦 環境需求(本地)

```bash
# Ubuntu 22.04 + ROS 2 Humble 已裝(主線環境)
# 額外裝:

# 1. PX4 Autopilot SITL
git clone https://github.com/PX4/PX4-Autopilot.git --recursive
cd PX4-Autopilot
bash ./Tools/setup/ubuntu.sh
make px4_sitl gazebo-classic

# 2. micro-XRCE-DDS Agent
sudo apt install ros-humble-micro-xrce-dds-agent

# 3. px4_msgs(放進 ros2_ws/src/)
cd ~/ros2_ws/src
git clone https://github.com/PX4/px4_msgs.git -b release/1.14
cd ~/ros2_ws
colcon build --packages-select px4_msgs

# 4. px4_ros_com(範例 + 工具)
cd ~/ros2_ws/src
git clone https://github.com/PX4/px4_ros_com.git -b release/1.14
cd ~/ros2_ws
colcon build --packages-select px4_ros_com
```

> ⚠️ **PX4 + ROS 2 版本綁很嚴**:Humble 對應 PX4 v1.14 + px4_msgs release/1.14。**版本錯配是新手雷區 #1**(訊息結構不對,bridge 出錯)。

---

## 🐛 預期會踩的雷(寫章節時逐條驗證)

整理業界踩過的雷,寫章節時會擴成完整解析:

1. **PX4 切到 OFFBOARD 後 1 秒內又跳回 MANUAL** — 沒持續送 `OffboardControlMode` 訊號(必須 ≥ 2 Hz)
2. **arm 失敗 `Vehicle is not in offboard mode`** — 順序錯,要先送 setpoint stream → 再切 OFFBOARD → 再 arm
3. **micro-XRCE-DDS Agent 連不上** — 預設用 UDP port 8888,雲端 ROSject 可能擋
4. **px4_msgs 編譯錯誤** — 版本不匹配 Humble + PX4 v1.14
5. **Gazebo Classic + PX4 SITL 啟動慢(20+ 秒)** — `make px4_sitl gazebo-classic` 第一次 build 跑很久

---

## 🔗 學習資源

- [PX4 + ROS 2 官方文件](https://docs.px4.io/main/en/ros2/user_guide.html)
- [px4_ros_com 範例](https://github.com/PX4/px4_ros_com)
- [Micro-XRCE-DDS Agent](https://micro-xrce-dds.docs.eprosima.com/)

---

## 🚦 開始之前

確認主線進度(至少要做完):
- ✅ Phase 01–04(通訊基礎)
- ✅ Phase 13(Action 進階)— 第 03 章必備
- ✅ Phase 16(TF2)— 第 03 章必備
- 推薦 ✅ Phase 17(Gazebo)— 知道 Gazebo 怎麼跑

---

## ⏭️ 從哪開始

主線完成後,第一章會是 **01-px4-bridge**(待寫):PX4 SITL + DDS bridge 設定。

> 這條支線目前只有 README 骨架。實際章節會在 gino 開始做時逐章補。
