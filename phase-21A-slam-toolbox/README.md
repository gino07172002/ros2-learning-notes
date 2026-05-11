# Phase 21A:SLAM with slam_toolbox

> 用 **slam_toolbox** online async 模式即時建圖。吃 Phase 17 的 Gazebo + TurtleBot3 提供 `/scan` 跟 `/odom`,持續產出 occupancy grid `/map` 與 `map → odom` TF。

**學完你會**:
- 寫 `slam_toolbox` 的 YAML 設定(odom_frame、map_frame、solver、map_update_interval)
- 在 launch 內延後啟動 SLAM,等 simulator 起來再吃資料
- 看穿 **message filter queue full / discarding** 這個 WSL 必踩的 SLAM 雷
- 區分 mapping / localization / lifelong 模式

**前置**:
- [Phase 17 Gazebo](../phase-17-gazebo/) — 提供 simulator 跟 turtlebot3
- [Phase 16 TF2](../phase-16-tf2/) — slam 的核心是維護 `map → odom` TF
- [Phase 06 Parameters](../phase-06-parameters/) — slam_toolbox 上百個 YAML 參數

**產出**:
- [`config/slam_async.yaml`](code/my_slam_demo/config/slam_async.yaml) — slam_toolbox 完整設定
- [`launch/slam_demo.launch.py`](code/my_slam_demo/launch/slam_demo.launch.py) — Gazebo + SLAM + 自動 cmd_vel

**環境**:☁️ TheConstructSim 推薦(雲端有 GPU,實際建出地圖) ｜ 💻 WSL2(可啟動結構驗證,但實際建圖受 WSL 沒 GPU 限制)

> ⚠️ **WSL 限制**:slam_toolbox 在 WSL2 沒 GPU 的環境,Ceres optimizer 處理慢,scan 進得比處理快,**message filter queue 持續 drop scan,實際 /map 不會有資料 / map→odom TF 不會發**。詳見雷 4。
> 雲端 ROSject 有 GPU,實測 SLAM 真的會跑出地圖。本章 WSL 部分只驗證「啟動結構正確 + slam node 在運作」,真實地圖在雲端產出。

---

## 🎓 觀念速成:SLAM 在做什麼

### 1. Occupancy Grid 是什麼?「**棋盤式的地圖**」

把地面切成一格格(預設 5cm × 5cm),每格只有三種狀態:

```
╔═══════════════════════════════╗
║  0   0   0  100 100  0   0   ║   100 = 障礙物(白色 → 黑色)
║  0  -1  -1 100  ?   ?  -1   ║     0 = 自由空間(可通行)
║  0  -1  -1  ?   ?   ?  -1   ║    -1 = 未探索(灰色)
║  0   0   0   0   0   0   0   ║
╚═══════════════════════════════╝
```

ROS 2 用 `nav_msgs/OccupancyGrid` 訊息表示,**Nav2 / RViz 都吃這個**。
比起「3D 點雲」這種地圖,occupancy grid 簡單但夠用:**對 2D 移動機器人(掃地、AGV)它就是事實標準**。

### 2. Localization vs Mapping vs SLAM 的差異

新手最容易混的三個詞:

| 模式 | 已知什麼 | 在做什麼 | 用什麼 |
|------|---------|---------|--------|
| **Localization**(定位) | 已有地圖 | 算「**我現在在地圖哪裡**」 | Nav2 amcl |
| **Mapping**(建圖) | 已知位置 | 把感測器資料**畫成地圖** | 一般不單用 |
| **SLAM**(同時定位 + 建圖) | 都不知道 | **邊走邊建地圖,同時算自己在哪** | 本章 slam_toolbox |

SLAM 是「雞生蛋蛋生雞」問題 — 沒地圖怎麼定位、沒定位怎麼建圖?
解法是:用 odometry 提供初步位置(來自 Phase 20A!) → 拿 lidar scan 比對前一刻的地圖 → 同時修正位置 + 擴充地圖。

### 3. slam_toolbox 的 4 個模式

| 模式 | 適合什麼 | 本章用的 |
|------|---------|---------|
| **Online async**(本章) | 即時跑、邊走邊建 | ✅ |
| **Online sync** | 邊走邊建但每 scan 等處理完 | — |
| **Localization** | 已建好地圖,只做定位(取代 amcl) | — |
| **Lifelong** | 長期跑、地圖會持續更新(店裡擺設變了會反映) | — |

本章只做 online async — 學完之後你會知道**怎麼從這裡換到其他模式**。

---

## 🤔 為什麼這章重要

**SLAM(Simultaneous Localization And Mapping)是移動機器人的靈魂技能**。沒地圖就沒 Nav2、沒 Nav2 就沒自動導航。

`slam_toolbox` 是 ROS 2 預設 SLAM 套件,業界 90% 用它(取代 ROS 1 時代的 gmapping)。Karto 的 graph-based SLAM 演算法,支援:
- **Online mapping** — 跑同時建圖(本章)
- **Localization** — 已有地圖時做定位
- **Lifelong** — 長期跑 + 動態更新地圖
- **Offline** — 從 ROS bag 後處理建圖

---

## 🏗️ 架構

```
                 ┌─────────────────────────────┐
                 │ Phase 17 Gazebo + tb3       │
                 │  publishes:                  │
                 │   /scan  (laser plugin)     │
                 │   /odom  (diff_drive plugin)│
                 │   /tf:   odom→base_footprint│
                 │         base_link→base_scan │
                 └────────────┬────────────────┘
                              │
                              ▼
                 ┌─────────────────────────────┐
                 │ slam_toolbox                 │
                 │  async_slam_toolbox_node     │
                 │  - subscribes /scan + tf     │
                 │  - publishes /map            │
                 │  - publishes map→odom tf     │
                 └────────────┬────────────────┘
                              │
                              ▼
                       /map (Occupancy Grid)
                       map → odom (TF)
```

`slam_toolbox` **不直接動 odom**,它讓 odom 維持原樣,只發 `map → odom` 的修正(localization-style)。這個設計 **ros 2 標準** — Nav2 完全依賴這個分工。

---

## 💻 重點檔案

### 1. config/slam_async.yaml — slam_toolbox 設定

完整見 [`config/slam_async.yaml`](code/my_slam_demo/config/slam_async.yaml)。

```yaml
slam_toolbox:
  ros__parameters:
    use_sim_time: true                    # 必須:跟 Gazebo 同步 /clock

    odom_frame: odom
    map_frame: map
    base_frame: base_footprint            # turtlebot3 用 base_footprint 而非 base_link
    scan_topic: /scan

    solver_plugin: solver_plugins::CeresSolver
    mode: mapping                         # mapping / localization / lifelong

    map_update_interval: 5.0              # 每 5 秒重算 occupancy grid
    transform_publish_period: 0.02        # 50 Hz 發 map→odom
    map_publish_period: 1.0               # 1 Hz 發 /map
    resolution: 0.05                      # 5cm 一格
    max_laser_range: 20.0
    minimum_time_interval: 2.0            # ⚠️ WSL 慢,放長避免 queue full
    transform_timeout: 1.0
    tf_buffer_duration: 60.0
```

### 2. launch/slam_demo.launch.py — 三段式 launch

```python
return LaunchDescription([
    # 1. Phase 17 Gazebo + turtlebot3
    IncludeLaunchDescription(...headless_demo.launch.py),

    # 2. SLAM 等 5 秒(讓 Gazebo + tf 先填上資料)
    # ⚠️ delay 太久(15s+)→ TF cache 跟 scan 會差太多時間,SLAM 找不到對應 TF
    TimerAction(period=5.0, actions=[
        Node(package='slam_toolbox', executable='async_slam_toolbox_node',
             parameters=[slam_yaml])]),

    # 3. 自動推 cmd_vel(15s 後)讓車自轉建圖
    TimerAction(period=15.0, actions=[
        ExecuteProcess(cmd=['ros2', 'topic', 'pub', '--rate', '10',
                            '/cmd_vel', 'geometry_msgs/Twist',
                            '{linear: {x: 0.05}, angular: {z: 0.5}}'])]),
])
```

關鍵設計:**TimerAction 把啟動順序拆開**,Gazebo→SLAM→車動,避免 race condition。

---

## 🚀 完整 Demo 流程

### Step 1:部署 + build

```bash
rm -rf ~/ros2_ws/src/my_slam_demo
cp -r /mnt/d/ros_learn/ros2-learning-notes/phase-21A-slam-toolbox/code/my_slam_demo \
      ~/ros2_ws/src/my_slam_demo
source /opt/ros/humble/setup.bash
cd ~/ros2_ws && colcon build --packages-select my_slam_demo
```

### Step 2:啟動

```bash
export TURTLEBOT3_MODEL=burger
ros2 launch my_slam_demo slam_demo.launch.py
```

啟動後預期 timeline:
- t=0:Gazebo + turtlebot3 起來
- t=5s:slam_toolbox 啟動
- t=15s:cmd_vel 自動發,車開始自轉

### Step 3a:☁️ TheConstructSim 步驟(推薦 — 雲端有 GPU,真的會建出地圖)

**雲端是這章的最佳環境**(WSL 沒 GPU 建不出地圖,雷 4 詳述)。

```bash
# 1. 在雲端 ROSject terminal 內 clone repo
cd ~/ros2_ws/src
git clone https://github.com/gino07172002/ros2-learning-notes.git
cp -r ros2-learning-notes/phase-21A-slam-toolbox/code/my_slam_demo .

# 2. 確認 turtlebot3 + slam_toolbox 已裝
ros2 pkg list | grep -E 'turtlebot3|slam_toolbox'
export TURTLEBOT3_MODEL=burger

# 3. Build + 跑(雲端不需要 gui:=false)
cd ~/ros2_ws
colcon build --packages-select my_slam_demo
source install/setup.bash
ros2 launch my_slam_demo slam_demo.launch.py

# 4. 切到 Tools → Gazebo 看車自己轉、Tools → Graphical Tools 開 RViz 看地圖建出來
```

**雲端預期看到**(WSL 看不到):
- `/map` topic 真的有 OccupancyGrid 資料(`ros2 topic echo /map --once` 拿得到)
- `map → odom` TF 真的會發
- RViz 內地圖隨著車自轉慢慢長出來

> 💡 **若你的 ROSject 沒預裝 slam_toolbox**:免費版 `apt install` 受限,改建立**「ROS 2 Navigation」分類的 ROSject**(預裝 Nav2 + slam_toolbox)。

---

### Step 3b:💻 WSL 驗證(部分驗證過)

```bash
# slam node 起來
ros2 node list | grep slam
# /slam_toolbox

# /map topic 有 publisher
ros2 topic info /map
# Type: nav_msgs/msg/OccupancyGrid
# Publisher count: 1

# slam 內部 topics
ros2 topic list | grep slam
# /slam_toolbox/feedback
# /slam_toolbox/graph_visualization
# /slam_toolbox/scan_visualization
# /slam_toolbox/update
```

✅ **以上四項在 WSL 都驗證過**

但下面兩項 **WSL 沒 GPU 跑不出來**(雷 4 詳述):
- ❌ `ros2 topic echo /map --once` 拿不到 occupancy grid 資料
- ❌ `ros2 run tf2_ros tf2_echo map odom` 看不到 map→odom TF

### Step 4:雲端 ROSject 完整驗證

ROSject 有 GPU,SLAM 真的會跑。流程一樣,額外開 RViz 看建圖:

```bash
ros2 launch my_slam_demo slam_demo.launch.py
# 另開 terminal
rviz2 -d /opt/ros/humble/share/slam_toolbox/config/slam_toolbox_default.rviz
```

預期:RViz 看到 turtlebot3 在 4×4m 圍場內自轉,/map 從黑慢慢染成白色(已知 free space)+ 灰色(未知)+ 黑線(牆)。

### Step 5:存地圖

建圖完成後可以存出來:

```bash
ros2 run nav2_map_server map_saver_cli -f ~/maps/my_room
# 產生 my_room.yaml + my_room.pgm
```

之後 Phase 22A Nav2 會用這份地圖做 localization。

---

## 🐛 常見雷

### ⚠️ 雷 1:`use_sim_time: true` 沒設,SLAM 跑了但永遠不出地圖

**症狀**:slam_toolbox 啟動,沒任何 error,但 `/map` 永遠沒 publisher,`map → odom` TF 永遠不發。

**原因**:slam_toolbox 預設 `use_sim_time: false`,接 wall clock。但 Gazebo 發的 `/scan` 戳的是 sim time。slam 內部 message filter:「我看的是 wall time 50,scan 戳 sim time 30」→ 永遠拒絕。

**解**:**yaml 內必設 `use_sim_time: true`**:
```yaml
slam_toolbox:
  ros__parameters:
    use_sim_time: true
```

### ⚠️ 雷 2:`base_frame` 設 base_link,實際 turtlebot3 用 base_footprint

**症狀**:`tf2 transform timeout`、`Lookup would require extrapolation`、SLAM 啟動失敗。

**原因**:turtlebot3 SDF 的 root link 叫 `base_footprint`(不是 `base_link`),`map → odom → base_footprint → base_link → ...`。slam_toolbox 預設 `base_frame: base_link`,它去找 `odom → base_link`,但 `odom` 直接連 `base_footprint`,中間少一段。

**解**:對 turtlebot3 系列設 `base_frame: base_footprint`。對自己的 robot 看 URDF 確認 root link。

### ⚠️ 雷 3:SLAM delay 太短(0–2s),Gazebo 還沒發 TF 就跑了

**症狀**:slam 啟動立刻噴 `Could not transform map → base_footprint`、`Failed to compute odom pose`。

**原因**:Gazebo 啟動約 15 秒(WSL),期間 TF 沒在發。slam 啟動太快,sub 上去什麼都沒收到,初始化失敗。

**解**:用 `TimerAction(period=5.0)` 讓 slam 等 5 秒。**也別等太久**(雷 4 解釋)。

### ⚠️ 雷 4:**WSL 沒 GPU,slam_toolbox queue full、不會出地圖**

**症狀(實測)**:
```
[INFO] [slam_toolbox]: Message Filter dropping message:
   frame 'base_scan' at time 13.504 for reason 'discarding message because the queue is full'
```
而且這條訊息**持續刷,scan 全部被丟,/map 沒資料**。

**原因**:slam_toolbox 的核心是 Ceres optimizer(graph SLAM),每筆 scan 進來要解最小化問題。WSL 沒 GPU 加上 ROS 2 callback queue 進得比處理快,內部 message_filter queue 滿了就 drop。

**短期解(WSL)**:
- 把 `minimum_time_interval` 從 0.5 拉到 2.0(每 2 秒才處理一次,降頻率)
- 在 world 設 `<real_time_factor>0.3</real_time_factor>`,sim time 跑 30% 速度,留出處理時間
- 但**這兩招都不會完全消滅 queue full**,最多讓 SLAM 偶爾跑出一筆 map

**真解**:
1. **用雲端 ROSject(有 GPU)** — 本章主推路線
2. **WSL 裝 CUDA + 切到 ros 2 nightly 的 GPU build**(複雜,不建議學習階段做)
3. **改 SLAM 演算法到輕量版**(`gmapping` for ROS 1 等價,但 ROS 2 沒了)

**業界體會**:這也是為什麼實機 robot 用 NVIDIA Jetson — SLAM 真的需要 GPU。Phase 27 部署實機就會碰到這個。

### ⚠️ 雷 5:scan timestamp earlier than transform cache

**症狀**:
```
Message Filter dropping message: ... 'the timestamp on the message is earlier than all the data in the transform cache'
```

**原因**:slam 啟動時 `/tf` 已經發了一陣子,TF cache 從 t=20s 開始。slam 訂 `/scan` 看到 t=5s 的舊 scan(latch 過來),scan 比 cache 老 → 拒絕。

**解**:slam 啟動延遲不要太久(< 10s),保持 scan 時間跟 TF cache 接近。或用 `tf_buffer_duration: 60.0` 拉長 buffer 容忍時序差。

### ⚠️ 雷 6:RMW middleware 衝突

**症狀**:slam_toolbox 跟 Gazebo 之間 topic 看得到但 echo 拿不到資料(類似 Phase 24 的 IPC 雷)。

**原因**:Gazebo plugin 跟 slam 用不同 rmw shared memory transport。

**解**:確認 `RMW_IMPLEMENTATION` 全部一致(預設 `rmw_fastrtps_cpp`)。本章 launch 內所有 node 共用同 process group,沒這問題;但跨 docker container 跑 SLAM 會撞到。

---

## 🎯 學到的關鍵概念

| 概念 | 一句話 |
|------|------|
| `slam_toolbox` 三模式 | mapping(建)/ localization(已有圖)/ lifelong(動態更新)|
| `map → odom` TF | slam 不動 odom,只發 map 對 odom 的修正,Nav2 標準 |
| Ceres solver | graph SLAM 後端,每幀都解優化問題,**WSL 沒 GPU 跑不動** |
| `use_sim_time: true` | 跟 Gazebo `/clock` 同步,沒設 SLAM 安靜失敗 |
| `base_frame` 因 robot 而異 | turtlebot3 是 `base_footprint`,別信預設 |
| TimerAction 啟動順序 | Gazebo → SLAM → cmd_vel,避免 race |
| WSL GPU 不足是 SLAM 的硬上限 | 教學用雲端 / 實機 Jetson,WSL 用來驗證結構 |

---

## 🌟 進階挑戰

1. **改用 localization 模式** — 把 mapping 跑出的 `.pgm` 載入,設 `mode: localization`,看 SLAM 用既有地圖只做定位
2. **Loop closure** — 帶車繞圍場一圈回到原點,看 slam_toolbox 偵測到 loop closure 並修正累積誤差
3. **錄 ROS bag** — `ros2 bag record /scan /odom /tf /tf_static`,之後改用 `offline_launch.py` 後處理建圖,避開 WSL GPU 問題
4. **改參數測 quality** — 玩 `resolution`(0.01–0.1),看建圖細節 vs CPU 負載 trade-off
5. **接 Nav2** — 把 `/map` 餵到 Phase 22A 的 Nav2 stack,實作完整自主導航

---

## 🔗 下一步

- **Phase 22A Nav2 入門** — 用本章建出來的地圖做自動導航
- **[Phase 17 Gazebo](../phase-17-gazebo/)** — 看 simulator side 的 plugin 設定
- **[Phase 16 TF2](../phase-16-tf2/)** — 深入 `map → odom → base_link` 三層 frame 的設計

---

## 📁 完整檔案結構

```
phase-21A-slam-toolbox/
├── README.md
├── code/
│   └── my_slam_demo/
│       ├── package.xml
│       ├── CMakeLists.txt
│       ├── config/
│       │   └── slam_async.yaml         ← slam_toolbox 完整設定
│       └── launch/
│           └── slam_demo.launch.py     ← Gazebo + SLAM + 自動 cmd_vel
└── images/                             ← (之後補:雲端 RViz 建圖截圖)
```
