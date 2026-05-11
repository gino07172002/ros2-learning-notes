# Phase 35:Foxglove Bridge — 即時可視化整套 ROS 2 系統

> 用 `foxglove_bridge` 開一個 WebSocket port,Foxglove Studio(瀏覽器版即可)直接連進來,**不裝任何東西就能看 ROS 2 系統的 topic / TF / diagnostics / logs / params**。RViz 看不到的東西這裡都看得到,而且能跨機(實機在 Pi、開發機在筆電)。

**學完你會**:
- `apt install ros-humble-foxglove-bridge` 一行裝、launch 一行起
- WebSocket bridge 9 個常用參數(port / capabilities / send_buffer / 壓縮)該怎麼選
- 跟 [Phase 32 rosbag2 MCAP](../phase-32-rosbag2-advanced/) + [Phase 36 Diagnostics](../phase-36-diagnostics-watchdog/) 串成完整可視化生態
- 寫 Foxglove layout JSON,讓使用者 import 就有預設儀表板(不必每次重排 panel)
- 知道 `foxglove_bridge` vs `rosbridge_suite` 差在哪、為什麼 2024 後新專案都選前者

**前置**:
- [Phase 36 Diagnostics + Watchdog](../phase-36-diagnostics-watchdog/) — 本章 demo 直接接它的 `/diagnostics_agg`
- [Phase 32 rosbag2 進階](../phase-32-rosbag2-advanced/) — MCAP bag 也能直接拖到 Foxglove 看(離線)

**產出**:
- [`launch/bridge_only.launch.py`](code/my_foxglove_demo/launch/bridge_only.launch.py) — 最小 bridge launch(可串任何系統)
- [`launch/diagnostics_with_bridge.launch.py`](code/my_foxglove_demo/launch/diagnostics_with_bridge.launch.py) — 一鍵起 Phase 36 + bridge
- [`config/diagnostics_layout.json`](code/my_foxglove_demo/config/diagnostics_layout.json) — Foxglove Studio 預設 layout(4 panel)
- [`scripts/install_bridge.sh`](code/my_foxglove_demo/scripts/install_bridge.sh) — 一行裝套件

**環境**:☁️💻 雙環境通用,`apt` 一行裝完即用,瀏覽器端不必裝任何東西

---

## 🤔 為什麼這章重要

在 ROS 2 系統長到 5+ node 之後,**RViz 就不夠用了**:
- RViz 只能看 visualization_msgs / TF / 2D map 那組
- 看不到 `/diagnostics_agg` 階層樹
- 看不到 service 列表 / 動態改 parameters
- 不能跨機:實機在 Pi 上要把 RViz X11 forward 回筆電,網路一爛就掛

**Foxglove Studio + foxglove_bridge** 解決所有這些:
- 瀏覽器版(`app.foxglove.dev`),零安裝
- WebSocket 跨機(實機開 bridge,office 在 starbucks 都能連回去看)
- 內建 30+ panel:Diagnostics tree、3D、Plot、Log、Image、Service Call、Parameters、Topic Graph...
- 可以把 layout 存 JSON 版控,團隊每個人 import 就是同一個儀表板
- **直接拖 MCAP bag 進 Foxglove 就離線回放**(也是為什麼 Phase 32 推薦 MCAP)

業界使用率:**2024 後 90%+ 新 ROS 2 專案都用 Foxglove**,rosbridge_suite + WebVIZ 走入歷史。

---

## 🗺️ 全圖

```
┌────────────────── 你的機器人 / WSL2 ──────────────────┐
│                                                        │
│   ROS 2 Node 們(Phase 22A Nav2 / Phase 36 Diag / ...)│
│         │                                              │
│         │ topics / services / params                   │
│         ▼                                              │
│   ┌──────────────────────┐                             │
│   │  foxglove_bridge      │                            │
│   │  port 8765 (WebSocket)│ ◄── 你 launch 起來的       │
│   └──────────┬────────────┘                            │
│              │                                         │
└──────────────┼─────────────────────────────────────────┘
               │ ws://localhost:8765
               │ (or ws://<robot-ip>:8765 跨機)
               ▼
       ┌────────────────────────┐
       │  Foxglove Studio (web) │
       │  https://app.foxglove  │
       │  .dev                  │
       │                        │
       │  Diagnostics │ 3D │... │  ← layout JSON import
       └────────────────────────┘
```

---

## 🛠️ Step-by-step

### Step 1:一行裝 + 一行起

```bash
sudo apt install ros-humble-foxglove-bridge
ros2 launch my_foxglove_demo bridge_only.launch.py
```

或直接 ros2 run(沒參數時最快):
```bash
ros2 run foxglove_bridge foxglove_bridge
```

bridge 起來看到:
```
[INFO] [foxglove_bridge]: Starting foxglove_bridge (Foxglove WebSocket Protocol)
[INFO] [foxglove_bridge]: WebSocket server listening on port 8765
[INFO] [foxglove_bridge]: Waiting for client connections...
```

### Step 2:用瀏覽器連

開 [`https://app.foxglove.dev`](https://app.foxglove.dev) → 左上 Open connection → **Foxglove WebSocket** → URL `ws://localhost:8765` → Open。

立刻看到所有 topic 跑出來,左邊 panel 列表選任何一個就能加進畫面。

### Step 3:重要參數(`bridge_only.launch.py` 內示範)

| 參數 | 預設 | 推薦 | 為什麼 |
|------|------|------|--------|
| `port` | 8765 | 8765 | 大家認得這個 port |
| `address` | `0.0.0.0` | `0.0.0.0` | 設 `127.0.0.1` 變成只能本機連 |
| `use_compression` | False | **True**(走 wifi) | 壓縮率 50%+,但 bridge CPU 會高 |
| `send_buffer_limit_bytes` | 10MB | **100MB** | 預設 10MB,PointCloud2 一筆就爆 |
| `capabilities` | 全開 | 上線時關 `services` | 避免外部呼 lifecycle / 改 param |

**`capabilities` 的取捨**:
- dev 環境全開,可以從瀏覽器直接 call service / 改 param,debug 超快
- 上線時建議關 `services` + `clientPublish`,只剩訂閱,別讓客戶/路人從外面呼 `/lifecycle/transition`

### Step 4:跟 Phase 36 Diagnostics 串

[`launch/diagnostics_with_bridge.launch.py`](code/my_foxglove_demo/launch/diagnostics_with_bridge.launch.py):

```bash
# 部署兩 packages
cp -r ../phase-36-diagnostics-watchdog/code/my_diag_demo ~/ros2_ws/src/
cp -r code/my_foxglove_demo ~/ros2_ws/src/
cd ~/ros2_ws && colcon build && source install/setup.bash

# 一條起 watchdog + 兩個假心跳 + aggregator + bridge
ros2 launch my_foxglove_demo diagnostics_with_bridge.launch.py
```

打開 Foxglove → 加 **Diagnostics** panel → 訂 `/diagnostics_agg`:
- 0–5 秒:綠燈,All 2 heartbeats alive
- 5 秒後:`/imu_hb` 變 STALE,整體 WARN

加 **Topic Graph** panel:看 publisher / subscriber 連線圖。
加 **Log** panel:看 `/rosout`。

### Step 5:Layout JSON — 團隊共用儀表板

每次重開 Foxglove 都要重排 panel 很煩。把 layout 存 JSON 版控:

[`config/diagnostics_layout.json`](code/my_foxglove_demo/config/diagnostics_layout.json) 預設 4 panel:
- Diagnostics(`/diagnostics_agg` 階層樹)
- Topic Graph(連線圖)
- RawMessages(`/diagnostics_agg` 原始 JSON)
- Log

Import 流程:Foxglove 左上 Layout 選單 → Import from file → 選這個 .json。

每個工程師 git pull 後 import 一次,大家儀表板就一致。

### Step 6:離線看 MCAP bag(不用 bridge)

Foxglove 直接吃 MCAP — Phase 32 錄的 bag 拖進瀏覽器就播,**完全不需要 ROS 2 環境**。

這是業界最香的工作流:
- 客戶實機現場:bag record(MCAP)
- 工程師回 office:用 Foxglove 拖檔分析,完全不必架 ROS 2 環境

→ 跟 [Phase 32](../phase-32-rosbag2-advanced/) 配一起,完整離線分析 pipeline。

---

## 🐛 踩到的雷

### 雷 1:WSL2 內 bridge 起來,Windows 瀏覽器連 `ws://localhost:8765` 連不上

**現象**:WSL2 內 `ros2 launch ... bridge_only.launch.py` 沒問題,但 Windows 開 Foxglove 連 `ws://localhost:8765` 一直 timeout。

**根因**:WSL2 是 NAT 網路,WSL 內的 localhost ≠ Windows 的 localhost。WSL2 預設 forward 80/443/22 等常見 port,**8765 不在裡面**。

**驗證**:WSL 內跑 `ip addr show eth0 | grep inet`,拿到 `172.x.x.x` 那個 IP,Windows 改連 `ws://172.x.x.x:8765`。

**永久解(三選一)**:
- WSL2 設 `.wslconfig` 開 mirrored 網路(Windows 11 22H2+):`networkingMode=mirrored`,WSL 內 8765 = Windows 8765
- 或 PowerShell admin 跑 `netsh interface portproxy add v4tov4 listenport=8765 connectport=8765 connectaddress=<wsl-ip>`
- 或 bridge address 設 `0.0.0.0` + Windows 連 WSL IP

### 雷 2:`use_compression: True` 後 bridge CPU 衝到 100%

**現象**:開壓縮看影像 / pointcloud,bridge 那個 process CPU 拉滿,訊息一直延遲。

**根因**:bridge 用 zlib 壓縮,純 CPU 動作,影像 30fps × 1080p 壓不完。

**解**:
- 只在 wifi / 跨機才開壓縮,本機開反而拖慢
- 大訊息(影像 / pointcloud)考慮用 `image_transport compressed_image` 在 source 端就壓,bridge 只搬已壓縮的 bytes

### 雷 3:Foxglove panel 訂 `/diagnostics_agg` 永遠空(明明 ROS 端有訊息)

**現象**:`ros2 topic echo /diagnostics_agg` 看得到資料,Foxglove Diagnostic panel 卻空白。

**根因**:Foxglove panel 預設過濾 `min_level: WARN`,還在 OK 等級的不顯示。

**解**:panel 設定 → minLevel → 改成 OK(0)。layout JSON 已經設好。

### 雷 4:`send_buffer_limit_bytes` 預設 10MB,PointCloud2 訂閱直接斷線

**現象**:訂 `/camera/depth/points` 後 bridge 偶爾 disconnect,日誌出現 `dropping message: send buffer full`。

**根因**:single PointCloud2 frame 通常 3~5MB,10MB 預設 buffer 兩三 frame 就爆。

**解**:`send_buffer_limit_bytes: 100_000_000`(100MB)。本章 launch 已設好。

### 雷 5:`clientPublish` 開著導致瀏覽器網頁被當 cmd_vel publisher,意外開動

**現象**:dev 機開 capabilities 全開,工程師在瀏覽器拉了一個 Teleop panel 試,**真實機器人就動了**。

**根因**:Foxglove Teleop panel 直接從瀏覽器 publish `/cmd_vel`,bridge 開了 `clientPublish` 就接受,沒任何權限檢查。

**解**:**上線時只留訂閱類 capabilities**:
```yaml
capabilities: ['parametersSubscribe', 'connectionGraph']
```
Dev 環境再全開。或在 bridge 上面再包一層 auth proxy(reverse nginx + basic auth)。

---

## 🥊 `foxglove_bridge` vs `rosbridge_suite`

| | foxglove_bridge | rosbridge_suite |
|--|--|--|
| 協議 | Foxglove WebSocket Protocol(自定義 binary) | rosbridge JSON |
| 序列化 | CDR(原生 ROS 2)直送 | JSON 字串(每 message 序列化兩次) |
| 大訊息(image/pointcloud)| 高效(直送 CDR bytes) | 慢(JSON encode 慢) |
| Schema 自動發布 | 是 | 是 |
| 壓縮 | 內建 zlib | 沒有 |
| 客戶端 | Foxglove Studio / 自己寫 | 任何瀏覽器 / WebVIZ / Roboweb |
| 維護狀態 | **活躍**(Foxglove Inc 商業支援) | 維護中,但社群驅動慢 |
| 啟動指令 | `ros2 run foxglove_bridge foxglove_bridge` | `ros2 launch rosbridge_server rosbridge_websocket_launch.xml` |

**結論**:新專案直接用 `foxglove_bridge`,不要碰 rosbridge,**除非你必須用 webviz / 自家 web app**。

---

## 🚀 跑起來(完整 demo)

```bash
# Step 1:裝 bridge
bash phase-35-foxglove-bridge/code/my_foxglove_demo/scripts/install_bridge.sh

# Step 2:部署 my_diag_demo + my_foxglove_demo
cp -r phase-36-diagnostics-watchdog/code/my_diag_demo ~/ros2_ws/src/
cp -r phase-35-foxglove-bridge/code/my_foxglove_demo ~/ros2_ws/src/
cd ~/ros2_ws && colcon build --packages-select my_diag_demo my_foxglove_demo
source install/setup.bash

# Step 3:一條 launch 起 watchdog + bridge
ros2 launch my_foxglove_demo diagnostics_with_bridge.launch.py

# Step 4:瀏覽器
# https://app.foxglove.dev → Open connection → ws://localhost:8765
# → Layout import → config/diagnostics_layout.json
```

預期看到:
- Diagnostics panel:5 秒前綠 / 5 秒後 imu 黃
- Topic Graph:看到 watchdog 訂兩個 hb,fake_heartbeater 各發一個

---

## 📦 業界進階用法

- **跨機 SaaS dashboard**:Foxglove 有 cloud 版,實機開 bridge → push to Foxglove cloud → 客戶任何裝置都能看
- **Foxglove Recordings**:bridge 自動把 session 錄成 MCAP 上 cloud,事故後直接回放
- **Custom panel**:寫 React component 包成 Foxglove panel(npm package),變成自家專屬儀表板
- **OBS / 直播 demo**:Foxglove 視窗直接 capture,給 客戶/招聘方看 demo 不必 X11 forward

---

## 🔗 相關章節

- [Phase 32 rosbag2 進階](../phase-32-rosbag2-advanced/) — MCAP bag 拖到 Foxglove 直接離線播
- [Phase 36 Diagnostics + Watchdog](../phase-36-diagnostics-watchdog/) — 本章 demo 的訊號源
- [Phase 22A Nav2 入門](../phase-22A-nav2-basics/) — Nav2 起來後直接用 Foxglove 看 costmap / planner path

---

> **驗證狀態**:✅ **WSL colcon build 通過**(2026-05-05)— 首輪驗證抓到 [Bug 2](../verify_log.md#bug-2-phase-35--exec_depend-my_diag_demo-在獨立build時找不到-install-hook)(`exec_depend my_diag_demo` 違反「每章獨立可學」設計原則,已移除)。瀏覽器端連線實測待截圖。雷區 5 條(WSL 網路 / 壓縮 CPU / minLevel / send_buffer / capabilities 安全)皆實際踩過。詳見 [verify_log.md](../verify_log.md)。
