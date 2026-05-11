# Phase 17:Gazebo Classic 整合(headless)

> 在 WSL 用 **headless Gazebo Classic 11**(沒 GUI)起一個小 world、spawn TurtleBot3,驗證 sensor topics + nav stack 必須的 TF 結構。後續 SLAM/Nav2 章節的 simulator 基礎。

**這章你將解鎖的業界技能**：
- **親手打造物理世界 (SDF World)**：不再依賴別人寫好的模擬環境，你將學會用 Gazebo 專屬的 XML 格式 (SDF)，從零刻劃出包含陽光、地板與圍牆的客製化測試場地。
- **無頭模式 (Headless) 穩定啟動**：精準避開新手最常踩的 `gzserver.launch.py` 缺乏 `ros_factory` 插件的世紀大雷，學會用正確的方式在背景啟動 Gazebo 伺服器，省下被 GUI 卡死的寶貴時間與運算資源。
- **動態召喚機器人 (`spawn_entity`)**：學會如何在世界已經運轉的狀態下，透過指令行或 Python 腳本，隨時將一台設定好的機器人動態「生成 (Spawn)」到指定的 X/Y/Z 座標上。
- **破解 URDF 與 SDF 的混淆迷宮**：徹底釐清為什麼 `turtlebot3_description` 裡的 URDF 只有空殼（沒有感測器數據），而 `turtlebot3_gazebo` 裡的 SDF 卻能發送光達資料。這是一個讓無數業界新手卡關超過三天的巨大盲點。

**前置**:
- [Phase 15 URDF](../phase-15-urdf/) — 機器人描述檔基礎
- [Phase 16 TF2](../phase-16-tf2/) — 驗證 spawn 後 TF tree 對不對

**產出**:
- [`worlds/empty_with_walls.world`](code/my_gazebo_demo/worlds/empty_with_walls.world) — 4×4m 圍牆 SDF world
- [`launch/headless_demo.launch.py`](code/my_gazebo_demo/launch/headless_demo.launch.py) — gazebo + rsp + spawn 三 node 整合

**環境**:💻 本機 WSL2(headless 部分)
> ☁️ TheConstructSim:**有 Gazebo GUI**,直接用網頁可視化更舒服。本機 WSL 沒 GPU/WSLg 限制,只跑 headless 文字驗證

> **GUI 部分這章不做** — 看實體機器人在 Gazebo 視窗跑來跑去要等使用者開 GUI(WSLg 或 Windows X server)。本章已 100% 驗證 headless,SLAM/Nav2 後續章節 launch 全自動,真的不太需要 GUI

---

## 🤔 為什麼這章重要

**所有後面 Track A(SLAM、Nav2)章節都需要一個 simulator**。
**業界的真實開發日常：Simulator (Gazebo) 就是工程師的「虛擬實驗室」。**
- **模擬器是開發的 90%，實機測試只是最後一哩路**：當你在開發 SLAM 建圖、微調 Costmap (代價地圖) 參數，或是 Debug 為什麼路徑規劃器 (Planner) 總是撞牆時，絕對不能拿造價百萬的真實機器人來不斷撞擊測試。所有的前期驗證，全部都在 Simulator 中以 10 倍速的迭代效率安全完成。
- **認清版本現實 (Gazebo Classic vs. Ignition/Harmonic)**：雖然 Gazebo Classic 即將在 2025 年迎來生命週期的終點 (EOL)，但殘酷的現實是，目前工業界最穩定且佔有率極高的 ROS 2 Humble，其預設綁定的物理引擎仍然是 Gazebo Classic 11。所以，搞懂它的脾氣，是你不可逃避的必修課。

這章主要解掉幾個 ROS+Gazebo 整合的常見雷,讓你後面 launch SLAM/Nav2 的時候不用每次重踩。

---

## 🏗️ 架構

```
                                        ┌── /clock              (Gazebo → ROS, sim time)
                                        ├── /cmd_vel            (ROS → diff_drive plugin)
   ┌────────────────────────────┐       ├── /odom               (diff_drive plugin → ROS)
   │ gazebo.launch.py(headless)│       ├── /scan               (laser plugin → ROS)
   │  ├─ gzserver               │ ───── ├── /imu                (imu plugin → ROS)
   │  ├─ libgazebo_ros_init     │       ├── /joint_states       (joint state plugin)
   │  ├─ libgazebo_ros_factory  │       ├── /tf, /tf_static
   │  └─ libgazebo_ros_force    │       └── /robot_description   (rsp)
   └────────────────────────────┘
              ▲
              │ spawn_entity.py -file model.sdf
              │
   ┌────────────────────────────┐
   │ TurtleBot3 burger SDF      │
   │  + diff_drive plugin       │
   │  + lds_lfcd_sensor plugin  │
   │  + imu plugin              │
   │  + joint_state plugin      │
   └────────────────────────────┘
```

`gazebo_ros_factory` 是關鍵:它提供 `/spawn_entity` service,launch 期間動態加機器人靠它。

---

## 💻 重點檔案

### 1. empty_with_walls.world — 自製 SDF world

完整見 [`worlds/empty_with_walls.world`](code/my_gazebo_demo/worlds/empty_with_walls.world)。

```xml
<sdf version="1.6">
  <world name="default">
    <include><uri>model://sun</uri></include>
    <include><uri>model://ground_plane</uri></include>

    <!-- 四面牆,4×4m 圍場 -->
    <model name="wall_north">
      <static>true</static>
      <pose>0 2 0.25 0 0 0</pose>
      <link name="link">
        <collision name="c">
          <geometry><box><size>4 0.1 0.5</size></box></geometry>
        </collision>
        <visual name="v">
          <geometry><box><size>4 0.1 0.5</size></box></geometry>
        </visual>
      </link>
    </model>
    <!-- ... 另三面牆 -->
  </world>
</sdf>
```

為什麼自己寫:`turtlebot3_world` 的 world 太複雜(雜物、家具)且慢。教學要重點在「light、ground、几面牆」就夠 SLAM 練習。

### 2. headless_demo.launch.py — 串起三個 node

完整見 [`launch/headless_demo.launch.py`](code/my_gazebo_demo/launch/headless_demo.launch.py)。

```python
# Gazebo headless:用 gazebo.launch.py(自帶 ros_init + ros_factory + ros_force_system)
gazebo = IncludeLaunchDescription(
    PythonLaunchDescriptionSource(
        os.path.join(get_package_share_directory('gazebo_ros'),
                     'launch', 'gazebo.launch.py')),
    launch_arguments={
        'world': world_file,
        'verbose': 'true',
        'gui': 'false',          # ← 不啟動 gzclient
    }.items(),
)

# robot_state_publisher 讀 turtlebot3_description 的 URDF(只用來發 TF / robot_description)
with open(tb3_urdf_for_rsp, 'r') as f:
    urdf_xml = f.read()
rsp = Node(
    package='robot_state_publisher',
    executable='robot_state_publisher',
    parameters=[{'robot_description': urdf_xml, 'use_sim_time': True}],
)

# 用 SDF spawn:這份才有完整 ros gazebo plugin
tb3_sdf = os.path.join(get_package_share_directory('turtlebot3_gazebo'),
                       'models', 'turtlebot3_burger', 'model.sdf')
spawn = Node(
    package='gazebo_ros',
    executable='spawn_entity.py',
    arguments=['-entity', 'burger', '-file', tb3_sdf,
               '-x', '0.0', '-y', '0.0', '-z', '0.01'],
)
```

**關鍵**:**robot_state_publisher 用 URDF**(發 `/robot_description` + TF tree),**spawn 用 SDF**(進 Gazebo 物理引擎)。兩者其實是同一台機器人的不同檔案格式,各自 ROS 整合層用不同。

---

## 🚀 完整 Demo 流程(WSL,驗證過)

### Step 1:部署 + build

```bash
rm -rf ~/ros2_ws/src/my_gazebo_demo
cp -r /mnt/d/ros_learn/ros2-learning-notes/phase-17-gazebo/code/my_gazebo_demo \
      ~/ros2_ws/src/my_gazebo_demo
source /opt/ros/humble/setup.bash
cd ~/ros2_ws && colcon build --packages-select my_gazebo_demo
```

### Step 2:啟動 + 驗證 topics / nodes

```bash
export TURTLEBOT3_MODEL=burger      # ⚠️ 必須,turtlebot3 系列 launch 都會檢查
ros2 launch my_gazebo_demo headless_demo.launch.py &
sleep 35     # gzserver + spawn 大概要 30 秒(WSL 沒 GPU 物理引擎慢)

ros2 topic list
ros2 node list
```

**驗證過的 topics**:
```
/clock
/cmd_vel
/imu
/joint_states
/odom
/parameter_events
/performance_metrics
/robot_description
/rosout
/scan
/tf
/tf_static
```

**驗證過的 nodes**:
```
/gazebo
/robot_state_publisher
/turtlebot3_diff_drive
/turtlebot3_imu
/turtlebot3_joint_state
/turtlebot3_laserscan
```

`/scan`、`/odom`、`/cmd_vel` 三個 SLAM/Nav2 必需的 topic 都已在發。`/tf` + `/tf_static` 帶完整 frame tree(odom→base_footprint→base_link→各 sensor)。

### Step 3:跑機器人(可選)

開另一個 terminal 推 `cmd_vel`:

```bash
ros2 topic pub --rate 10 /cmd_vel geometry_msgs/Twist \
    "{linear: {x: 0.1}, angular: {z: 0.3}}"
```

`/odom` 的 `pose.position` 應該開始變化。`/scan` 的 ranges 因車轉而變動。

### Step 4:GUI(留給使用者本機)

如果你有 WSLg 或想看畫面:

```bash
ros2 launch my_gazebo_demo headless_demo.launch.py gui:=true
# 或單獨開 gzclient 連到 server
gzclient
```

---

## ☁️ TheConstructSim 完整步驟(推薦 — 不用裝 Gazebo)

雲端 ROSject **預設就有 Gazebo GUI**,且有正確的 OpenGL 後端,**比 WSL 順很多**。

### Step 1:建立 ROSject

1. 登入 [TheConstructSim](https://app.theconstructsim.com/) → **ROSjects** → **Create New ROSject**
2. ROS Distro 選 **ROS 2 Humble**
3. 進入 ROSject 後,**Tools** 面板下方會有 Gazebo viewer

### Step 2:複製本章 code 到雲端

在雲端 terminal:
```bash
cd ~/ros2_ws/src
git clone https://github.com/gino07172002/ros2-learning-notes.git
cp -r ros2-learning-notes/phase-17-gazebo/code/my_gazebo_demo .
```

### Step 3:確認 turtlebot3 已裝(雲端通常已有)

```bash
ros2 pkg list | grep turtlebot3
# 預期看到: turtlebot3_description, turtlebot3_gazebo, turtlebot3_msgs ...
export TURTLEBOT3_MODEL=burger
```

### Step 4:Build + 跑

```bash
cd ~/ros2_ws
colcon build --packages-select my_gazebo_demo
source install/setup.bash

# 跟 WSL 不同:雲端不要加 gui:=false,直接看 GUI
ros2 launch my_gazebo_demo headless_demo.launch.py
```

### Step 5:看 Gazebo viewer

切到 **Tools → Gazebo** 面板,你會看到 turtlebot3 已經 spawn 在世界中。
另開一個 terminal 試:
```bash
ros2 topic list | grep -E '/scan|/odom|/cmd_vel'
ros2 topic pub /cmd_vel geometry_msgs/Twist '{linear: {x: 0.2}}' --once
# Gazebo 內車子應該會前進
```

> 💡 **與本機差異**:
> - 雲端不需要 `gui:=false`(它有 X11)
> - 雲端 spawn 比較快(預先暖機過)
> - 雲端 GPU 比 WSL 強,實際機器人移動會比較順
> - **雲端免費版連線約 1 hr,做完馬上儲存**

---

## 🐛 常見雷

### ⚠️ 雷 1:`gzserver.launch.py` 缺 ros_factory,`/spawn_entity` service 不存在

**症狀**:用 `gzserver.launch.py` 啟動後,`spawn_entity.py` 卡住:
```
Waiting for service /spawn_entity, timeout = 30
[ERROR] Service /spawn_entity unavailable. Was Gazebo started with GazeboRosFactory?
```

**原因**:`gzserver.launch.py` 預設只載 `libgazebo_ros_init.so`,**沒載 `libgazebo_ros_factory.so`**。spawn_entity service 由後者提供。

**解**:用 `gazebo.launch.py`,它預設載入完整 plugin set:
```bash
ls /proc/$(pgrep gzserver)/cmdline   # 應該看到 -slibgazebo_ros_factory.so
```

或者你堅持用 gzserver.launch.py,要明確傳 `init:=true factory:=true force_system:=true`。

### ⚠️ 雷 2:用 turtlebot3_description URDF spawn 沒 sensor

**症狀**:gzserver、spawn 都 OK,但 `ros2 topic list` 沒 `/scan` `/odom` `/imu`。

**原因**:你用 `turtlebot3_description/urdf/turtlebot3_burger.urdf`(純 URDF,**沒 `<gazebo>` 標籤**,沒 ros plugin)當 spawn 來源。光達掃不到、odometry 不發、IMU 不發。

**解**:spawn 用 `turtlebot3_gazebo/models/turtlebot3_burger/model.sdf`,這份 SDF 帶完整 ros plugin。robot_state_publisher 還是讀 URDF(它只需要 link/joint 結構發 TF)。**兩個檔案各司其職**:
| 檔案 | 用途 | 由誰讀 |
|------|------|------|
| `turtlebot3_description/urdf/turtlebot3_burger.urdf` | TF tree 結構 | robot_state_publisher |
| `turtlebot3_gazebo/models/turtlebot3_burger/model.sdf` | 物理 + sensor plugin | gazebo spawn |

### ⚠️ 雷 3:`TURTLEBOT3_MODEL` 沒 export,launch 馬上炸

**症狀**:`ros2 launch ...` 立刻死,有 `TURTLEBOT3_MODEL is not set` 訊息。

**原因**:turtlebot3 系列(包含我們依賴的 turtlebot3_description)在 setup.bash 有檢查 `TURTLEBOT3_MODEL` 環境變數,沒設拒絕 import。

**解**:export 進 `~/.bashrc`:
```bash
echo 'export TURTLEBOT3_MODEL=burger' >> ~/.bashrc
source ~/.bashrc
```

### ⚠️ 雷 4:gzserver 啟動超慢(WSL 30s+)

**症狀**:`ros2 launch` 好幾秒沒輸出,以為卡住,結果只是慢。

**原因**:Gazebo 第一次跑會去 `models.gazebosim.org` 下載 sun / ground_plane 模型,約 5–10 秒。WSL 沒 GPU,物理引擎初始化又慢。

**解**:
- 第一次跑等 30s 是正常的
- 之後 model cache 在 `~/.gazebo/models/`,啟動會快一些(15–20s)
- 加 `verbose:=true` 看詳細進度,確認真的在跑

### ⚠️ 雷 5:`gzclient` 在 WSLg 卡圖、卡黑

**症狀**:`gui:=true` 後 gzclient 視窗黑色 / 凍結 / 顯示異常。

**原因**:Gazebo Classic 11 對 Windows GPU 透傳(WSLg)不友善,frame buffer 經常 corrupt。

**解**:
1. **headless 不用 GUI**(本章主路線)
2. 或 export `LIBGL_ALWAYS_SOFTWARE=1` 強制軟體 render(慢但能用)
3. 或裝 X server(VcXsrv/X410)接一接

但對 SLAM/Nav2 demo,**RViz 才是看內容的工具**,gzclient 視覺只是看 robot 真的在轉。本章策略:headless 跑物理 + 之後 RViz 看 sensor。

### ⚠️ 雷 6:`/clock` use_sim_time 沒設,SLAM/Nav2 時間戳混亂

**症狀**:之後接 SLAM/Nav2 時 `tf2 transform timeout` 一直跳,或 `extrapolation into the past` 錯誤。

**原因**:Gazebo 發 `/clock` 提供 sim time,但 ROS node 預設用 wall time。tf 訊息戳的是 sim time,但 listener 用 wall time 找 transform → 永遠對不上。

**解**:**所有跟 Gazebo 互動的 node 都要設 `use_sim_time: True`**:
```python
parameters=[{'use_sim_time': True}]
```

本章 launch 已給 robot_state_publisher 設好,SLAM/Nav2 章節要繼續沿用這慣例。

---

## 🎯 學到的關鍵概念

- **啟動腳本的抉擇 (`gazebo.launch.py` vs `gzserver.launch.py`)**：永遠記得使用前者。因為後者閹割了 `ros_factory` 插件，會導致你的 `spawn_entity` 指令永遠等不到服務上線而卡死。
- **URDF 與 SDF 的愛恨情仇**：URDF 負責向 ROS 系統報告「我有這些骨架」；而 SDF 則是負責告訴 Gazebo「我的骨架上有這些會受重力影響的鐵塊，以及會發射雷射光的感測器插件」。
- **生成方式的差異 (`-file` vs `-topic`)**：使用 `-file` 可以直接把強大的 SDF 檔案塞進 Gazebo；而使用 `-topic` 則只能拉取被轉譯過的 URDF 空殼。這決定了你的機器人到底是活的還是死的。
- **效能救星 (`gui:=false`)**：在沒有獨立顯卡的 WSL 或 Docker 環境中，這是保命符。強制關閉沉重的 3D 渲染視窗，讓運算資源全數投入核心的物理模擬。
- **時間的相對論 (`use_sim_time: True`)**：當你開啟了 Gazebo，整個 ROS 世界的時間就必須以 Gazebo 發布的 `/clock` 為準。如果有任何一個 Node 忘了加這個設定而繼續使用真實世界的時間，那 TF 樹的時間戳記就會徹底錯亂，演算法全盤崩潰。
- **模型來源的陷阱 (`turtlebot3_description` 不含 Plugin)**：千萬不要拿這個資料夾裡的檔案去 Spawn 機器人，你會得到一台沒有感測器、沒有動力的植物車。實體模擬必須使用 `turtlebot3_gazebo` 目錄下的 SDF。

---

## 🌟 進階挑戰

1. **自寫 SDF model** — 把 Phase 15 你做的 mobile robot URDF 加上 `<gazebo>` plugin 區段,變成可在 Gazebo 跑的 model
2. **多機器人** — 改 launch 同時 spawn 兩台 burger,各自 `/burger1/scan`、`/burger2/scan`
3. **動態障礙物** — 在 world 內加 actor 行人,SLAM 章節練「動態避障」
4. **改用新版 Gazebo (Harmonic)** — 把 launch 換成 `ros_gz_sim`,跨入 Gazebo 後 EOL 時代
5. **Gazebo + ros2_control** — 接 Phase 18 ros2_control,在 simulator 內驗證自寫的 controller plugin

---

## 🔗 下一步

- **Phase 21A SLAM** — 吃本章的 `/scan` + `/odom` 開始建圖
- **Phase 22A Nav2** — 在本章 world 內自動導航
- **[Phase 18 ros2_control](../phase-18-ros2-control/)** — 把 Gazebo 接 ros2_control,用真正的 controller 驅動 turtlebot3

---

## 📁 完整檔案結構

```
phase-17-gazebo/
├── README.md
├── code/
│   └── my_gazebo_demo/
│       ├── package.xml
│       ├── CMakeLists.txt
│       ├── worlds/
│       │   └── empty_with_walls.world      ← 4×4m 圍牆 SDF
│       └── launch/
│           └── headless_demo.launch.py     ← gazebo + rsp + spawn 三 node
└── images/                                 ← (之後補:gzclient 截圖)
```
