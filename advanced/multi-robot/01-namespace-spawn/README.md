# 01. Namespace + Spawn — 多機器人隔離與部署

> 在同一個 Gazebo 世界 spawn **3 台 turtlebot3**,每台用不同 namespace(`tb1` / `tb2` / `tb3`)隔離 topic 跟 TF。完成後可以**同時鍵盤遙控其中任一台**,其他不受影響。

**學完你會**:
- 用 namespace 隔離多台機器人的 topic(`/tb1/cmd_vel` / `/tb2/cmd_vel` / `/tb3/cmd_vel`)
- 用 `tf_prefix` 隔離 TF tree(`tb1/base_link` vs `tb2/base_link`)
- 寫 launch file 同時 spawn 3 台 turtlebot 在 Gazebo
- 看穿 ROS 2 「`/tf` `/tf_static` 是全域的」這個多機特有雷
- 用 `ros2 topic list` 看到每台獨立的 topic 命名空間

**前置**:
- [Phase 11 Launch 進階](../../../phase-11-launch-files-advanced/) — namespace + IncludeLaunchDescription
- [Phase 16 TF2](../../../phase-16-tf2/) — 知道 TF tree 怎麼長
- [Phase 17 Gazebo](../../../phase-17-gazebo/) — 會用 spawn_entity

**產出**:[`code/multi_robot_demo/`](code/multi_robot_demo/) — launch file + 3 台 turtlebot 整合

**環境**:☁️ TheConstructSim(turtlebot3 ROSject)推薦 / 💻 本機 WSL2(3 台 + Gazebo CPU 重)

---

## 📍 為什麼這章是多機系統的入門關卡

單機(Phase 22A 一台 turtlebot3 跑 Nav2)沒有 namespace 問題。**一旦兩台以上**,所有原本「**全域唯一**」的東西都會打架:

| 衝突 | 單機 | 多機問題 |
|------|------|---------|
| Topic | 全域 `/cmd_vel` | 兩台同時訂同一條 → 兩台同時動 |
| TF tree | 全域 `base_link` | 兩台都發 `world → base_link`,後到的覆蓋先到的 |
| Service | 全域 `/odom` | server 名衝突,只有一個會被連到 |
| Node 名 | 全域 `slam_toolbox` | 同名 Node 啟動時互相 kill |

**解法核心**:**namespace 包住所有 entity** + **TF tree 加 prefix**。

---

## 🎯 設計目標

```
        Gazebo World
        ┌─────────────────────────────────────┐
        │                                     │
        │    🤖 tb1                            │
        │   (-2, -2)    🤖 tb2                 │
        │              ( 0,  0)    🤖 tb3      │
        │                         ( 2,  2)    │
        │                                     │
        └─────────────────────────────────────┘

Topic 結構(每台一份):
  /tb1/cmd_vel        /tb2/cmd_vel        /tb3/cmd_vel
  /tb1/odom           /tb2/odom           /tb3/odom
  /tb1/scan           /tb2/scan           /tb3/scan

TF tree(以 tb1 為例):
  world → tb1/odom → tb1/base_link → tb1/lidar_link
  world → tb2/odom → tb2/base_link → tb2/lidar_link
  world → tb3/odom → tb3/base_link → tb3/lidar_link
```

---

## ⚠️ 關鍵知識:`/tf` 是全域的(ROS 2 的特例)

**這是新手雷區 #1**。即使設了 `namespace="tb1"`,**TF 訊息預設仍會發到全域 `/tf` topic**,不是 `/tb1/tf`。

原因:`tf2` 為了讓「**機器人 A 看得到 機器人 B 的位置**」,刻意設計 TF 是全域共享的。

帶來的問題:**3 台 turtlebot 都發 `world → base_link` → 互相覆蓋**。

**解法**:
1. **TF 訊息內的 frame_id 加 prefix**(`tb1/base_link` 而非 `base_link`)
2. **可選**:把 `/tf` `/tf_static` remap 到 namespace 內(進階,本章先用 prefix 解決)

---

## 💻 步驟 1:設計 launch file

完整見 [`code/multi_robot_demo/launch/spawn_three.launch.py`](code/multi_robot_demo/launch/spawn_three.launch.py)。

核心結構:

```python
def spawn_robot(name: str, x: float, y: float):
    """為一台 turtlebot 生出完整套件:robot_state_publisher + spawn_entity"""

    # 讀 turtlebot3 URDF + 加 tf_prefix
    urdf_path = os.path.join(
        get_package_share_directory('turtlebot3_description'),
        'urdf', 'turtlebot3_burger.urdf')
    with open(urdf_path) as f:
        urdf_content = f.read()
    # 把所有 link / joint name 前面加 prefix
    urdf_content = urdf_content.replace('link_name="', f'link_name="{name}/')
    # 實務上更乾淨的做法是用 xacro 帶 prefix 參數,本章從簡

    # 1. robot_state_publisher 在 namespace 下(發 TF + URDF)
    rsp = Node(
        package='robot_state_publisher',
        executable='robot_state_publisher',
        namespace=name,
        parameters=[{
            'robot_description': urdf_content,
            'frame_prefix': f'{name}/',   # ← 關鍵:TF frame 加 prefix
        }],
        output='screen',
    )

    # 2. spawn_entity 把機器人放進 Gazebo
    spawn = Node(
        package='gazebo_ros',
        executable='spawn_entity.py',
        arguments=[
            '-topic', f'/{name}/robot_description',
            '-entity', name,
            '-x', str(x),
            '-y', str(y),
            '-z', '0.01',
            '-robot_namespace', name,    # ← Gazebo 內 topic 也加 namespace
        ],
        output='screen',
    )

    return [rsp, spawn]


def generate_launch_description():
    # 啟動 Gazebo(空場景)
    gazebo = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(get_package_share_directory('gazebo_ros'),
                         'launch', 'gazebo.launch.py')))

    # 3 台機器人,各自不同位置
    tb1 = spawn_robot('tb1', -2.0, -2.0)
    tb2 = spawn_robot('tb2',  0.0,  0.0)
    tb3 = spawn_robot('tb3',  2.0,  2.0)

    # 用 TimerAction 延遲第二、三台 spawn,避免 race
    return LaunchDescription([
        gazebo,
        *tb1,
        TimerAction(period=3.0, actions=tb2),
        TimerAction(period=6.0, actions=tb3),
    ])
```

**重點**:
- `frame_prefix='tb1/'` 是 robot_state_publisher 的關鍵 — 它會把 URDF 內所有 link 名加 prefix 再發 TF
- `-robot_namespace tb1` 是 Gazebo plugin 的關鍵 — 它讓 `gazebo_ros_diff_drive` 之類的 plugin 在 `/tb1/cmd_vel` 而不是 `/cmd_vel` 訂閱
- TimerAction 延遲 spawn — 第一次同時 spawn 3 台 Gazebo 會 race

---

## 🚀 步驟 2:Build + 跑

### ☁️ TheConstructSim 步驟

```bash
# 1. clone repo
cd ~/ros2_ws/src
git clone https://github.com/gino07172002/ros2-learning-notes.git
cp -r ros2-learning-notes/advanced/multi-robot/01-namespace-spawn/code/multi_robot_demo .

# 2. 確認 turtlebot3 已裝
ros2 pkg list | grep turtlebot3
export TURTLEBOT3_MODEL=burger

# 3. Build + 跑
cd ~/ros2_ws
colcon build --packages-select multi_robot_demo
source install/setup.bash
ros2 launch multi_robot_demo spawn_three.launch.py
```

### 💻 本機 WSL2 步驟

```bash
cp -r /mnt/d/ros_learn/ros2-learning-notes/advanced/multi-robot/01-namespace-spawn/code/multi_robot_demo ~/ros2_ws/src/
cd ~/ros2_ws
colcon build --packages-select multi_robot_demo
source install/setup.bash
export TURTLEBOT3_MODEL=burger
ros2 launch multi_robot_demo spawn_three.launch.py
```

---

## 🎯 步驟 3:驗證 namespace 隔離

```bash
# 1. 看到 3 套獨立 cmd_vel
ros2 topic list | grep cmd_vel
# 預期:
#   /tb1/cmd_vel
#   /tb2/cmd_vel
#   /tb3/cmd_vel

# 2. 看到 3 套獨立 odom
ros2 topic list | grep odom
# 預期:
#   /tb1/odom
#   /tb2/odom
#   /tb3/odom

# 3. TF frame 看 3 套
ros2 run tf2_tools view_frames
# 開啟生成的 frames.pdf,應該看到 tb1/base_link、tb2/base_link、tb3/base_link
# 三個 root 各自連到 world(或 odom)
```

---

## 🚀 步驟 4:遙控其中一台,其他不動

開 3 個 terminal,各自 teleop 不同 namespace:

```bash
# Terminal 1 — 控 tb1
ros2 run teleop_twist_keyboard teleop_twist_keyboard \
  --ros-args -r __ns:=/tb1

# Terminal 2 — 控 tb2
ros2 run teleop_twist_keyboard teleop_twist_keyboard \
  --ros-args -r __ns:=/tb2

# Terminal 3 — 控 tb3
ros2 run teleop_twist_keyboard teleop_twist_keyboard \
  --ros-args -r __ns:=/tb3
```

**預期**:每個 terminal 獨立控制對應的車,**互不干擾**。

---

## 🐛 常見雷

### 雷 1:3 台 spawn 同時下指令,Gazebo 卡住或漏 spawn

**症狀**:Gazebo 只看到 1–2 台,或啟動時 console 出 `[spawn_entity-N] [ERROR]: Service /spawn_entity not available`。

**原因**:Gazebo 的 spawn_entity service 在 Gazebo 內部 ready 之前就被呼叫 → race。

**解**:用 `TimerAction(period=3.0, actions=spawn_entity_node)` 延遲 spawn,確保 Gazebo ready。第一台可以馬上 spawn(等 Gazebo 自己 ready),第二、三台延後。

---

### 雷 2:`frame_prefix` 設了但 RViz 看不到 3 台機器人

**症狀**:RViz 設 Fixed Frame `world`,只看到一台或全部疊在原點。

**原因**:**TF prefix 沒有真的影響 URDF 內的 link**,只影響 robot_state_publisher 發出的 frame name。如果 URDF 內的 link 名 hardcode 沒帶 prefix,Gazebo plugin 發 odometry TF 時用的還是 `base_link`,**會跟其他機器人互相覆蓋**。

**解**(本章用):URDF 修改 — 用 `sed` 把所有 link 名加 prefix。
**進階解**:用 `xacro:macro` 把整個機器人 URDF 包成 macro,參數帶 `prefix`,每台 spawn 時帶不同 prefix 展開。

---

### 雷 3:`/tf` 跟 `/tf_static` 是全域,namespace 沒擋住

**症狀**:即使設了 `namespace=tb1`,`ros2 topic list` 內 `/tf` 仍是全域只有一條,沒看到 `/tb1/tf`。

**原因**:**ROS 2 設計上 TF 就是全域共享**(讓多機可以互看位置),即使 namespace 也不會分開。

**這不是 bug,是 feature**。我們用 `frame_prefix` 區分 frame name,就能在同一個 `/tf` topic 上共存:
- `tb1/base_link → tb1/odom`
- `tb2/base_link → tb2/odom`
- `tb3/base_link → tb3/odom`

frame name 不同就不會衝突。

---

### 雷 4:3 台 turtlebot3 同時跑 Nav2,WSL CPU 100% 卡死

**症狀**(下一章 02-fleet-coordination 會碰到):跑 multi-Nav2,WSL 沒 GPU CPU 100%,所有 lifecycle 卡在 `Configuring`。

**解**:
- 雲端跑(GPU 比較順)
- 或降 controller_frequency 到 5 Hz(原本 20)
- 或先只測 1–2 台,確認結構對,再擴 3 台

本章還沒到 Nav2,3 台 spawn + teleop 在 WSL 還能跑。

---

### 雷 5:Gazebo plugin 沒帶 robot_namespace,topic 都跑到全域

**症狀**:`ros2 topic list` 看到 3 套 robot_description 但 cmd_vel 只有一條 `/cmd_vel`。

**原因**:`turtlebot3_description` 的 URDF 內 `<gazebo>` plugin tag 預設沒設 `<namespace>`,Gazebo 把所有 plugin 發到全域 topic。

**解**:`spawn_entity.py` 的 `-robot_namespace tb1` 參數會把 plugin 自動加上 namespace。本章 launch 已包含。

---

## 🎯 學到的關鍵概念

- **多機系統的 namespace 隔離 = 三層**:topic / TF prefix / Gazebo plugin
- `frame_prefix` 是 robot_state_publisher 的關鍵參數,加 TF prefix 不改 URDF
- `-robot_namespace` 是 spawn_entity.py 的關鍵參數,讓 Gazebo plugin 訂正確 namespace
- `/tf` 跟 `/tf_static` 是全域的,**不是 bug,設計如此**(讓多機可互看)
- TimerAction 延後 spawn,避免 Gazebo race

---

## 🌟 進階挑戰

1. **改用 xacro:** 用 xacro macro 包整個 turtlebot URDF,參數帶 prefix。比 sed 替換乾淨
2. **加第 4、5 台**(看 Gazebo 物理引擎能撐到幾台)
3. **開 RViz 用 fixed_frame=world,看 3 台 TF tree** 共存
4. **加 SLAM(每台一份 slam_toolbox)** — 進階,需要 namespace 內也跑 SLAM,看雷 3 怎麼處理 `/tf` global

---

## 下一步

- [02. Fleet Coordination](../02-fleet-coordination/)(待寫) — 寫 FleetManager 派任務給多台

---

> **驗證狀態**:⏸ 純文字草稿(2026-05-05) — Launch file 結構照 Phase 17 + 11 同模式,雷 1–5 從業界經驗整理。雲端 / WSL 實際驗證後升級成 ✅。
