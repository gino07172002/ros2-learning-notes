# Phase 10：Launch Files 基礎 🚀

**學完你會**：🌟 用一行 `ros2 launch` 同時啟動多個 Node、自動 remap topic、注入參數、從 YAML 載入設定、讓使用者從 CLI 傳參數。再也不用開 5 個 terminal 手動 `ros2 run`。

**前置準備**：
- [Phase 08 Custom Interfaces](../phase-08-custom-interfaces/) — 本章 launch 會啟動 `phase08_pkg/smart_brake_v2`

**產出目標**：
- [`code/my_cpp_pkg/launch/01_minimal.launch.py`](code/my_cpp_pkg/launch/01_minimal.launch.py) — 最簡單範例
- [`code/my_cpp_pkg/launch/02_remap_and_params.launch.py`](code/my_cpp_pkg/launch/02_remap_and_params.launch.py) — 多 Node + remap
- [`code/my_cpp_pkg/launch/03_yaml_params.launch.py`](code/my_cpp_pkg/launch/03_yaml_params.launch.py) — 從 YAML 載入
- [`code/my_cpp_pkg/launch/04_with_args.launch.py`](code/my_cpp_pkg/launch/04_with_args.launch.py) — 接受 CLI 參數

**環境**：☁️ TheConstructSim + 💻 本機 WSL 雙環境通用。

---

## 🤔 為什麼需要 Launch File

回想你之前每次跑 demo 要做的事：

```bash
# Terminal 1: turtlesim
ros2 run turtlesim turtlesim_node

# Terminal 2: smart_brake_v2 + remap
ros2 run phase08_pkg smart_brake_v2 --ros-args -r cmd_vel:=/turtle1/cmd_vel

# Terminal 3: fake_lidar
python3 ~/fake_lidar.py 0.5
```

**每個 Phase 都這樣**——而且每次重啟還要重打。

Launch File 把這些濃縮成一行：

```bash
ros2 launch phase10_pkg 02_remap_and_params.launch.py
```

業界 ROS 系統的 Nav2 啟動 launch 一行帶起 **20+ 個 Node**，靠的就是 launch file。

---

## 對照你熟悉的東西

| 你熟悉的 | ROS 2 Launch File |
|---------|-------------------|
| `docker-compose.yml` | 啟動多 container 的編排檔 |
| `systemd .service` | 系統啟動時自動跑某些程式 |
| Bash 啟動腳本 | 一次跑多個 background process |
| **ROS 2 Launch File** | 上面三個的綜合體 + ROS 特有的 remap/param |

**重要**:ROS 2 的 launch file **是 Python 程式**(不是 YAML 或 XML)。所以可以寫 if/for/讀環境變數,比 docker-compose 靈活很多。

---

## 🕵️ 終端機偵探課:看 ROS 系統內已有哪些 launch file

ROS 2 套件本身**就帶 launch file**(turtlebot3、Nav2、MoveIt 都是)。寫自己的 launch 之前先看現有的:

```bash
# 看某個套件提供哪些 launch file
ros2 launch turtlebot3_gazebo --show-args turtlebot3_world.launch.py

# 列出所有套件的 launch file(可能很多)
ros2 pkg prefix turtlebot3_gazebo
ls $(ros2 pkg prefix turtlebot3_gazebo)/share/turtlebot3_gazebo/launch/
```

預期看到 turtlebot3_gazebo 帶的 launch file:
```
empty_world.launch.py
turtlebot3_house.launch.py
turtlebot3_world.launch.py
...
```

**直接跑業界 launch file 看效果**:
```bash
# 這一行 launch 起 Gazebo + turtlebot3 spawn,內部跑 5+ 個 Node
export TURTLEBOT3_MODEL=burger
ros2 launch turtlebot3_gazebo empty_world.launch.py

# 開另一 terminal 看實際起了多少 node
ros2 node list
# /gazebo
# /robot_state_publisher
# /turtlebot3_diff_drive
# /turtlebot3_imu
# /turtlebot3_joint_state
# ...
```

**💡 劃重點**:
- **業界絕對沒有人會手動 `ros2 run` 幾十次**：在正式的 Production 系統（例如自動駕駛汽車或工廠的物流機器人）中，系統通常由數十甚至數百個微小的 Node 組合而成。所有的啟動、參數配置與相依性控制，全都依賴嚴謹編寫的 Launch File 來一鍵自動完成。
- **Launch File 的本質是強大的 Python 程式**：這意味著你不但可以寫 `if/else` 條件判斷（例如「如果有偵測到光達硬體才啟動感知節點」），還能直接去**讀開源大神的 Launch File 當作最佳教材**。你只需要用 `ros2 pkg prefix <pkg>` 找到套件安裝路徑，進去 `share/launch/` 裡面挖寶，就能學到很多業界的高階寫法。
- **循序漸進的學習路徑**：這章我們會先教你寫出最陽春的 Launch File，接著一步步加上 Topic 的 Remap、注入參數 (Parameters)，甚至讓它能接收使用者從終端機傳入的指令 (CLI args)。等你破關這章，你也能寫出跟 turtlebot3 一樣專業且功能豐富的啟動腳本。

---

## 📝 四個漸進範例

### 範例 1：最小 Launch（[01_minimal.launch.py](code/my_cpp_pkg/launch/01_minimal.launch.py)）

```python
from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description():
    return LaunchDescription([
        Node(
            package='turtlesim',
            executable='turtlesim_node',
            name='my_turtle',
        ),
    ])
```

**規則**：
- Launch file 必須提供 `generate_launch_description()` 函數
- 函數回傳 `LaunchDescription` 物件，內含一個 list of actions
- `Node(...)` 是最常用的 action（啟動 ROS Node）

執行：
```bash
ros2 launch phase10_pkg 01_minimal.launch.py
```

跟 `ros2 run turtlesim turtlesim_node` 效果一樣，但多了 `name='my_turtle'`——節點名變成 `my_turtle` 而不是 `turtlesim`。

---

### 範例 2：多 Node + Remap + Parameters（[02_remap_and_params.launch.py](code/my_cpp_pkg/launch/02_remap_and_params.launch.py)）

```python
return LaunchDescription([
    Node(package='turtlesim', executable='turtlesim_node', name='turtlesim'),

    Node(
        package='phase08_pkg',
        executable='smart_brake_v2',
        name='smart_brake_v2',
        output='screen',                                  # log 印出來
        remappings=[('cmd_vel', '/turtle1/cmd_vel')],     # ⚠️ 順序：(程式裡寫的, 真實 topic)
        parameters=[{'use_sim_time': False}],
    ),
])
```

**💡 劃重點**：
- `remappings` 是 list of tuples — `(source, destination)`
- `parameters` 可以是 dict（在 launch 內 inline 寫）或 file path（下個範例）
- `output='screen'`：log 印到 terminal。**不加會被吞掉**——你看不到任何 RCLCPP_INFO。

執行 + 預期效果：
```bash
ros2 launch phase10_pkg 02_remap_and_params.launch.py
```
你會看到：
- TurtleSim 視窗跳出
- smart_brake_v2 在 terminal 印 `smart_brake_v2 ready (mode=ENABLED)`
- 因為沒接 fake_lidar，`closest_obstacle_distance` 是預設 100m（Clear），smart_brake_v2 會持續送 max_speed=0.5 給 turtlesim — **烏龜會慢慢往右跑直到撞牆**

> 雷：`remappings` 順序跟 CLI 的 `--ros-args -r A:=B` **完全相反**。Launch 裡是 `(A, B)`，CLI 也是 `A:=B`，剛好一致。但很多新手會記反——只要記「左邊是程式、右邊是真實」。

---

### 範例 3：從 YAML 載入參數（[03_yaml_params.launch.py](code/my_cpp_pkg/launch/03_yaml_params.launch.py)）

```python
import os
from ament_index_python.packages import get_package_share_directory

def generate_launch_description():
    pkg_share = get_package_share_directory('phase10_pkg')
    config_path = os.path.join(pkg_share, 'config', 'smart_brake.yaml')

    return LaunchDescription([
        Node(
            package='phase08_pkg',
            executable='smart_brake_v2',
            parameters=[config_path],         # 直接傳檔案路徑
        ),
    ])
```

對應的 [`config/smart_brake.yaml`](code/my_cpp_pkg/config/smart_brake.yaml)：
```yaml
smart_brake_v2:
  ros__parameters:
    use_sim_time: false
```

**為什麼要大費周章用 YAML 檔，而不直接在 Launch 裡寫 inline 字典就好？**
- **支援多場景的靈活切換**：在實際開發中，你可能會需要「本機開發環境 (dev)」、「雲端測試環境 (staging)」與「實車正式環境 (prod)」三種不同的設定。把參數抽離成 YAML 後，你只需要維護唯一一份 Launch File，啟動時根據當下環境載入對應的 YAML 檔即可，極度乾淨俐落。
- **對團隊夥伴更友善**：很多時候，負責調校機器人參數的人可能是控制工程師或 QA 測試員，他們不一定懂 Python。提供一份結構清晰的 YAML 讓他們調整 PID 參數或極速限制，會比讓他們去改動 Launch 原始碼安全且方便得多。
- **避免不必要的 CI/CD 觸發**：在正規專案中，修改 Launch 程式碼可能會觸發漫長的自動化測試流水線 (CI)。但如果只是微調參數，單純修改 YAML 檔案就不會牽動到核心程式的編譯流程。

**這也是為什麼業界 ROS 系統高達 90% 的參數都交由 YAML 管理**——你去翻開 Nav2 或 MoveIt 的開源專案，它們的 Launch 腳本幾乎全都是從肥大的 YAML 設定檔裡將參數讀進來的。

> ⚠️ **YAML node 名稱必須跟 launch 內 `Node(name='xxx')` 一致**——打錯字會靜默失敗（用預設值）。

---

### 範例 4：CLI 接受參數（[04_with_args.launch.py](code/my_cpp_pkg/launch/04_with_args.launch.py)）

```python
from launch.actions import DeclareLaunchArgument, ExecuteProcess
from launch.substitutions import LaunchConfiguration

def generate_launch_description():
    obstacle_arg = DeclareLaunchArgument(
        'obstacle_distance',
        default_value='0.5',
        description='Distance (m) of fake obstacle in front of robot',
    )
    distance = LaunchConfiguration('obstacle_distance')

    return LaunchDescription([
        obstacle_arg,
        Node(...),  # turtlesim
        Node(...),  # smart_brake_v2
        ExecuteProcess(
            cmd=['python3', '/home/gino/fake_lidar.py', distance],
            output='screen',
        ),
    ])
```

**新東西**：
- `DeclareLaunchArgument` 宣告 CLI 參數
- `LaunchConfiguration('name')` 取出參數值（注意這是 lazy object，不是 str）
- `ExecuteProcess` 跑非 ROS 標準的命令（這裡是 fake_lidar.py 腳本）

執行：
```bash
# 用預設值 0.5
ros2 launch phase10_pkg 04_with_args.launch.py

# 自訂 0.3 公尺障礙物
ros2 launch phase10_pkg 04_with_args.launch.py obstacle_distance:=0.3

# 看 launch 接受哪些 args
ros2 launch phase10_pkg 04_with_args.launch.py --show-args
```

最後一行會顯示：
```
Arguments (pass arguments as '<name>:=<value>'):
    'obstacle_distance':
        Distance (m) of fake obstacle in front of robot
        (default: '0.5')
```

---

## 🚀 實際 Demo 流程

### Step 1：部署套件

#### ☁️ TheConstructSim
```bash
cd ~/ros2_ws/src
git clone https://github.com/gino07172002/ros2-learning-notes.git
ln -s ros2-learning-notes/phase-10-launch-files-basics/code/my_cpp_pkg phase10_pkg
```

#### 💻 本機 WSL2
```bash
cp -r /mnt/d/ros_learn/ros2-learning-notes/phase-10-launch-files-basics/code/my_cpp_pkg \
      ~/ros2_ws/src/phase10_pkg
sed -i 's|<name>my_cpp_pkg</name>|<name>phase10_pkg</name>|' ~/ros2_ws/src/phase10_pkg/package.xml
sed -i 's|project(my_cpp_pkg)|project(phase10_pkg)|' ~/ros2_ws/src/phase10_pkg/CMakeLists.txt
```

### Step 2：build

```bash
cd ~/ros2_ws
colcon build --packages-select phase10_pkg
source install/setup.bash
```

### Step 3：依序試四個範例

```bash
# 1) 最小範例
ros2 launch phase10_pkg 01_minimal.launch.py

# 2) 多 Node + remap (推薦看這個)
ros2 launch phase10_pkg 02_remap_and_params.launch.py

# 3) YAML 參數
ros2 launch phase10_pkg 03_yaml_params.launch.py

# 4) CLI args
ros2 launch phase10_pkg 04_with_args.launch.py obstacle_distance:=0.3
```

### Step 4：用 launch 工具觀察

```bash
# 看 launch 接受哪些 args
ros2 launch phase10_pkg 04_with_args.launch.py --show-args

# 列出 phase10_pkg 提供的 launch files
ls ~/ros2_ws/install/phase10_pkg/share/phase10_pkg/launch/

# Ctrl+C 殺 launch 會把所有它啟動的 Node 一起殺掉
```

---

## 🐛 常見雷

### 雷 1：`output='screen'` 沒寫，看不到 log
```python
Node(..., output='screen')   # ✅ log 印到 terminal
Node(...)                     # ❌ log 被 launch 吃掉
```

### 雷 2：YAML node 名打錯
```yaml
# ❌ 'smart_brake_v2'  →  Node(name='smart_brake_v2_node')，不一致 → 靜默失敗
smart_brake_v2:
  ros__parameters: ...
```
**對照 `Node(name='xxx')` 嚴格一致**。

### 雷 3：remappings 順序記反
```python
remappings=[('cmd_vel', '/turtle1/cmd_vel')]   # ✅ 程式裡寫的 → 真實 topic
remappings=[('/turtle1/cmd_vel', 'cmd_vel')]   # ❌ 反了 — 沒有 effect
```
記法：「**程式 → 真實**」（左到右）。

### 雷 4：launch file 沒被安裝
```cmake
# CMakeLists.txt 必須有
install(DIRECTORY launch DESTINATION share/${PROJECT_NAME})
```
忘了寫，`ros2 launch` 找不到檔案，會報「No such file」。

### 雷 5：`get_package_share_directory` 找不到套件
通常是套件還沒 build 或 source。**用 `colcon build` 先把套件裝好再啟動 launch**。

### 雷 6：launch 檔案名稱不以 `.launch.py` 結尾
```
my_launch.py        # ❌ ros2 launch 找不到
my.launch.py        # ✅ 正確
```
慣例必須以 `.launch.py` 結尾。

### 雷 7：Node executable 名稱拼錯
```python
Node(package='phase08_pkg', executable='smart_brake_v2')   # ✅
Node(package='phase08_pkg', executable='SmartBrakeV2')     # ❌
```
是 CMakeLists.txt 內 `add_executable(smart_brake_v2 ...)` 的 target 名字，不是類別名。

---

## 🎯 學到的關鍵概念

- **Launch File 是 Python**：可寫條件、迴圈、讀環境變數
- **必須提供 `generate_launch_description()`** 回傳 LaunchDescription
- **三個常用 action**：`Node`、`ExecuteProcess`、`IncludeLaunchDescription`（下章學）
- **Remap 順序**：(程式裡寫的, 真實 topic) — 跟 CLI `-r A:=B` 一致
- **參數來源**：inline dict、YAML 檔案、launch arg
- **`output='screen'`** 看 log 必加
- **檔名必須以 `.launch.py` 結尾**

---

## 🌟 進階挑戰

1. **改 04 用 launch arg 控制 fake_lidar.py 的距離**——已做；嘗試加第二個 arg `enable_brake`，false 時不啟動 smart_brake_v2
2. **用環境變數**：`os.environ.get('ROBOT_NAME', 'default_robot')` 動態決定 Node 名稱
3. **寫 launch file 啟動 rqt_graph + 你的系統**：開機就自動看通訊圖
4. **for 迴圈啟動多隻烏龜**（ROS 2 Multi-Robot 入門）：launch 內用 `for i in range(3)` 啟動三個 turtlesim Node + 各自的 namespace

---

## 👣 下一步去哪？

- [Phase 11 — Launch Files 進階](../phase-11-launch-files-advanced/)：IncludeLaunchDescription（嵌套 launch）、event_handlers（A 起來後再啟動 B）、條件啟動

---

## 📁 完整檔案結構

```
phase-10-launch-files-basics/
├── README.md
└── code/
    └── my_cpp_pkg/
        ├── package.xml
        ├── CMakeLists.txt
        ├── launch/
        │   ├── 01_minimal.launch.py
        │   ├── 02_remap_and_params.launch.py
        │   ├── 03_yaml_params.launch.py
        │   └── 04_with_args.launch.py
        └── config/
            └── smart_brake.yaml
```
