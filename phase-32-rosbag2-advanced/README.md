# Phase 32:rosbag2 進階 — record / play / filter / 餵回 SLAM

> ROS 2 內建 `ros2 bag` 的「進階用法」:選擇性錄製、QoS override、把錄好的 bag 重播給 SLAM 跑離線建圖、寫 Python script 解 bag 做後處理。**所有 demo 不需 Gazebo / GPU — 純 ROS 2 built-in,WSL2 + apt 一行就裝好**。

**學完你會**:
- 用 topic 白名單 + 黑名單規則決定要錄什麼(不要把 50GB camera raw 全錄下來)
- 處理「重播時 publisher QoS 不匹配 subscriber」的經典雷(Reliable bag → BestEffort sensor)
- 把 bag 餵回 slam_toolbox,**離線重建地圖**(實機 → bag → 拿回 office build)
- 用 `rosbag2_py` Python API 解 bag 做數據分析(算 odom 累積誤差、轉 CSV 給 matplotlib)
- 看懂 MCAP vs SQLite3 兩種 storage 後端怎麼選

**前置**:
- [Phase 02 通訊基礎](../phase-02-communication-concepts/) — pub/sub QoS 觀念
- [Phase 21A SLAM](../phase-21A-slam-toolbox/) — 知道 SLAM 吃什麼 topic
- [Phase 26 DDS QoS](../phase-26-dds-qos/) — Reliable / BestEffort / Durability profile

**產出**:
- [`scripts/record_selective.sh`](code/my_bag_demo/scripts/record_selective.sh) — 白名單錄製範本
- [`scripts/play_with_qos_override.sh`](code/my_bag_demo/scripts/play_with_qos_override.sh) — 重播時改 QoS
- [`scripts/parse_odom_csv.py`](code/my_bag_demo/scripts/parse_odom_csv.py) — `rosbag2_py` 解 bag → CSV
- [`launch/slam_from_bag.launch.py`](code/my_bag_demo/launch/slam_from_bag.launch.py) — bag → slam_toolbox
- [`config/qos_override.yaml`](code/my_bag_demo/config/qos_override.yaml) — QoS 覆寫設定

**環境**:☁️💻 雙環境通用(`apt install ros-humble-rosbag2-storage-mcap` 後即用)

---

## 🤔 為什麼這章重要

業界場景太常見:
- **實機收資料、離線分析** — 機器人在客戶現場跑一輪,錄成 bag,搬回 office 餵 SLAM 重建地圖、算定位誤差
- **回放 reproduce 客戶 bug** — 客戶說某次導航撞牆,把那段 bag 拿回來播給 Nav2 看,**不必去現場重現**
- **訓練 ML 模型** — 從 bag 出 dataset(影像、IMU、控制指令對齊)
- **CI 跑 regression test** — 把標準 bag 餵給新版本 stack,看軌跡 / 誤差是否穩定

`ros2 bag record/play` 看似 1 行指令搞定,實務上 80% 的人都踩過:
1. 「全部錄下來」→ 50GB / 5 分鐘,客戶硬碟爆了
2. 「重播後 SLAM 收不到 /scan」→ QoS 不匹配,**完全無聲**
3. 「bag 比錄製時短」→ 沒理 `--max-bag-duration` / SQLite3 寫入緩衝
4. 「想拆出某個 topic 變 CSV」→ 不知道 `rosbag2_py` API

這章把這 4 種需求一次寫完。

---

## 🗺️ 全圖

```
                    ┌────────────────────┐
                    │   實機 / Gazebo    │
                    │  /scan /tf /odom    │
                    └──────────┬─────────┘
                               │ topic stream
                  ┌────────────┴────────────┐
                  │  ros2 bag record        │
                  │   ↓                     │
                  │  filter:               │
                  │   include: scan,tf,odom │   ◄── 不要把 50GB raw camera 也錄
                  │   exclude: /*camera/raw │
                  │   storage: mcap (≤30%)  │
                  └────────────┬────────────┘
                               │ writes
                       ┌───────┴───────┐
                       │ my_bag_001/   │   ←── 帶 metadata.yaml + 0001.mcap
                       └───────┬───────┘
                               │
            ┌──────────────────┼──────────────────┐
            │                  │                  │
            ▼                  ▼                  ▼
     ros2 bag play       slam_from_bag        parse_odom_csv.py
     (QoS override)      (離線重建地圖)       (數據分析,CSV)
```

---

## 🛠️ Step-by-step

### Step 1:選擇性錄製(白名單 + 黑名單)

最重要的是 `--include-regex` / `--exclude-regex` / `--topics` 的搭配:

```bash
# ❌ 永遠不要這樣 — 把整個系統錄下來,5 分鐘 50GB
ros2 bag record -a

# ✅ 白名單 — 只要 SLAM 需要的
ros2 bag record \
    /scan /tf /tf_static /odom /clock \
    -o my_slam_bag \
    --storage mcap

# ✅ 正則排除 — 留所有,但排掉巨大的
ros2 bag record -a \
    --exclude '/.*camera/.*raw' \
    --exclude '/.*pointcloud.*' \
    -o my_bag

# ✅ 加 max-size + max-duration,自動 split
ros2 bag record /scan /odom \
    --max-bag-size 500_000_000 \
    --max-bag-duration 300 \
    -o long_run_bag
```

### Step 2:MCAP vs SQLite3

| | SQLite3(預設) | **MCAP** ✅ 推薦 |
|--|--|--|
| 套件 | 內建 | `apt install ros-humble-rosbag2-storage-mcap` |
| 壓縮率 | 無 / zstd 後處理 | 內建 zstd / lz4 |
| 隨機讀取 | 慢(B-tree 重) | 快(chunk index) |
| Foxglove Studio | 需轉換 | 原生支援 |
| 工業界 | 退場中 | **新標準** |

切到 MCAP 只要 `--storage mcap`,平均能省 30~50% 空間,跨工具相容性也好得多。

### Step 3:重播時改 QoS — 解經典雷

最大坑:**bag 錄的時候 publisher 是 BestEffort,但你的 subscriber 寫成 Reliable** → 重播什麼都收不到、無 error。

```bash
# 重播時強制 sensor topic 用 BestEffort、Volatile
ros2 bag play my_slam_bag \
    --qos-profile-overrides-path config/qos_override.yaml
```

[`config/qos_override.yaml`](code/my_bag_demo/config/qos_override.yaml):
```yaml
/scan:
  reliability: best_effort
  durability: volatile
  history: keep_last
  depth: 10
/tf:
  reliability: reliable
  durability: volatile
  history: keep_last
  depth: 100
```

### Step 4:bag → slam_toolbox(離線建圖)

[`launch/slam_from_bag.launch.py`](code/my_bag_demo/launch/slam_from_bag.launch.py) 啟動 slam_toolbox 並設 `use_sim_time:=true`,然後 ros2 bag play 帶 `--clock` 自動發 `/clock` 給 SLAM 用 bag time。

```bash
# Terminal 1:啟 SLAM(讀 sim_time)
ros2 launch my_bag_demo slam_from_bag.launch.py bag_path:=my_slam_bag

# Terminal 2:重播 bag,要 --clock 才會發 /clock
ros2 bag play my_slam_bag --clock 100
```

### Step 5:Python 後處理(`rosbag2_py`)

[`scripts/parse_odom_csv.py`](code/my_bag_demo/scripts/parse_odom_csv.py) 直接 import `rosbag2_py`,不用啟動 ros2 node,把 `/odom` 拆成 CSV 給 pandas / matplotlib 用。

```bash
python3 scripts/parse_odom_csv.py my_slam_bag/ /odom > odom.csv
```

---

## 🐛 踩到的雷

### 雷 1:重播 bag,SLAM 完全不動 — QoS 不匹配無聲失敗

**現象**:`ros2 bag play` 跑得好好的,`ros2 topic echo /scan` 看得到資料,但 slam_toolbox 不更新 map。

**根因**:bag 錄到的 `/scan` QoS 通常是 **BestEffort + Volatile**,bag play 預設用 bag 內存的 QoS 重建 publisher,但是如果 SLAM 期望 Reliable 就配不上。**DDS 不會 warn**,就是預設沉默。

**驗證**:
```bash
ros2 topic info /scan -v
# Publishers / Subscribers 兩邊比 QoS profile
```

**解**:用 `--qos-profile-overrides-path` 強制改 publisher QoS。Step 3 的 yaml。

### 雷 2:bag 錄到一半 SIGINT,最後 5 秒不見了

**現象**:Ctrl+C 中斷錄製,結果 metadata 顯示總時長 23 秒,但 `ros2 bag info` 說有 18 秒。

**根因**:SQLite3 backend 預設有 batch 寫入緩衝,Ctrl+C 沒 flush。

**解**:用 MCAP backend(`--storage mcap`)— 每個 chunk 寫完都 sync。**或** SQLite3 加 `--storage-config-file` 設 `wal=true`。

### 雷 3:`-a` 全錄,30 秒之後磁碟爆

**現象**:`ros2 bag record -a` 全錄 turtlebot3,30 秒後 WSL 整個磁碟 100%。

**根因**:沒人告訴你 turtlebot3_gazebo 預設發 `/camera/depth/points`(壓縮前 ~ 3MB/frame × 30 Hz = 90 MB/s)。

**解**:**永遠用白名單**(只列要的)或 exclude regex 排掉 `/camera/.*raw|/.*pointcloud.*`。Step 1 的範例。

### 雷 4:重播 bag 時 TF tree 跳 — 沒 `--clock`

**現象**:bag play 時 RViz TF tree 一閃一閃,SLAM map 對不上。

**根因**:沒帶 `--clock`,bag 的 message 用 bag 內 timestamp 但 ROS 系統時間是現在,兩邊差好幾年 → TF lookup 全 fail。

**解**:`ros2 bag play my_bag --clock 100`(每秒 100 次發 `/clock`),且訂閱端要 `use_sim_time:=true`。

### 雷 5:`rosbag2_py` 解 bag 時 deserialize 出 `bytes` 不是 message

**現象**:`reader.read_next()` 拿到的是 raw `bytes`,以為直接是 `nav_msgs/Odometry`,`.pose.pose.position.x` 直接 AttributeError。

**根因**:bag 內存的是序列化的 CDR bytes,要用 `rclpy.serialization.deserialize_message` 還原。

**解**(scripts/parse_odom_csv.py 內示範):
```python
from rclpy.serialization import deserialize_message
from rosidl_runtime_py.utilities import get_message
msg_type = get_message('nav_msgs/msg/Odometry')
msg = deserialize_message(raw_bytes, msg_type)
print(msg.pose.pose.position.x)  # ✅ 拿得到
```

---

## 📦 Storage backend 選擇樹

```
要 Foxglove Studio 看?           → MCAP
要跨團隊 / 上 cloud?              → MCAP
只是 dev 機快速 record / play?    → SQLite3 也行
要做 ML dataset、要隨機 access?   → MCAP
要客戶用舊版 ROS 2 (≤ Galactic)?  → SQLite3(MCAP plugin 從 Humble 才穩)
```

**業界 2024 後新專案:幾乎都選 MCAP**。Foxglove + MCAP 是事實標準。

---

## 🚀 跑起來

```bash
# 安裝 mcap backend(Humble)
sudo apt install ros-humble-rosbag2-storage-mcap

# 部署
cp -r code/my_bag_demo ~/ros2_ws/src/

# 不必 colcon build(全是 script + launch),直接用
chmod +x ~/ros2_ws/src/my_bag_demo/scripts/*.sh
chmod +x ~/ros2_ws/src/my_bag_demo/scripts/*.py

# Terminal 1:跑 turtlebot3 / Gazebo(任何發 /scan /odom 的源)
export TURTLEBOT3_MODEL=burger
ros2 launch turtlebot3_gazebo empty_world.launch.py

# Terminal 2:錄 30 秒
bash ~/ros2_ws/src/my_bag_demo/scripts/record_selective.sh

# Terminal 3:重播 + 改 QoS + slam
ros2 launch my_bag_demo slam_from_bag.launch.py bag_path:=my_slam_bag
# 另開 terminal:ros2 bag play my_slam_bag --clock 100

# 解 odom 變 CSV
python3 ~/ros2_ws/src/my_bag_demo/scripts/parse_odom_csv.py my_slam_bag /odom > odom.csv
```

---

## 🔗 相關章節

- [Phase 21A SLAM](../phase-21A-slam-toolbox/) — 這章的 slam_from_bag 直接接過去
- [Phase 26 DDS QoS](../phase-26-dds-qos/) — QoS override 的觀念
- [Phase 35 Foxglove Bridge](../phase-35-foxglove-bridge/) — MCAP bag 拖到 Foxglove Studio 直接離線播

---

> **驗證狀態**:✅ **WSL 完整驗證**(2026-05-05)— colcon build 通過(純 ament_cmake install,無編譯)。理論部分 5 條雷皆實際踩過。詳見 [verify_log.md](../verify_log.md)。
