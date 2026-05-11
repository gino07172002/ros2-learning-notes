# Phase 11：Launch Files 進階 🚀

**學完你會**：🌟 嵌套 launch（呼叫別的 launch file）、根據事件觸發動作（A 起來才啟動 B）、條件啟動（debug 模式才開 rqt）、多機器人 namespace 隔離。這些是業界 ROS 系統 launch 必備技能。

**前置準備**：
- [Phase 10 Launch Files 基礎](../phase-10-launch-files-basics/) — 必須先懂基礎
- [Phase 08 Custom Interfaces](../phase-08-custom-interfaces/) — 本章 launch 會啟動 smart_brake_v2

**產出目標**：4 個進階範例 launch file
- [`01_include.launch.py`](code/my_cpp_pkg/launch/01_include.launch.py) — 嵌套 launch
- [`02_event_handler.launch.py`](code/my_cpp_pkg/launch/02_event_handler.launch.py) — 事件驅動啟動
- [`03_conditional.launch.py`](code/my_cpp_pkg/launch/03_conditional.launch.py) — 條件啟動
- [`04_groups_namespace.launch.py`](code/my_cpp_pkg/launch/04_groups_namespace.launch.py) — 多機器人 namespace

**環境**：☁️ TheConstructSim + 💻 本機 WSL 雙環境通用。

---

## 🤔 為什麼有進階章

Phase 10 教的「Node + remap + param」可以啟動單一系統。但業界 ROS 系統有更複雜的需求：

| 需求 | Phase 10 解法 | Phase 11 解法 |
|------|--------------|---------------|
| 我的系統由 5 個團隊各自的 launch 組成 | ❌ 一份 launch 全包 | ✅ `IncludeLaunchDescription` |
| 必須等 A 起來再啟 B（避免 race） | ❌ 沒辦法 | ✅ `OnProcessStart` event handler |
| 同一份 launch 給 dev/prod 切換 | ⚠️ 寫兩份 | ✅ `IfCondition` + arg |
| 多台機器人各自獨立 topic 不混 | ❌ 寫多份 launch | ✅ `GroupAction` + `PushRosNamespace` |

---

## 📝 四個進階範例

### 範例 1：IncludeLaunchDescription — 嵌套 launch

**完整檔案**：[`01_include.launch.py`](code/my_cpp_pkg/launch/01_include.launch.py)

```python
from launch.actions import IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource

def generate_launch_description():
    phase10_share = get_package_share_directory('phase10_pkg')
    other_launch = os.path.join(phase10_share, 'launch', '02_remap_and_params.launch.py')

    return LaunchDescription([
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(other_launch),
        ),
        Node(package='rqt_graph', executable='rqt_graph', name='debug_graph'),
    ])
```

**業界場景**：
```
bringup.launch
├── perception.launch     (相機團隊維護)
├── localization.launch   (定位團隊維護)
├── planning.launch       (規劃團隊維護)
└── visualization.launch  (UI 團隊維護)
```

每個子 launch 各自獨立可跑、可測試。bringup 只負責「組起來」。

**Nav2 / MoveIt 都這樣做**——它們的主 launch 內含 10+ 個 IncludeLaunchDescription。

---

### 範例 2：Event Handler — 啟動順序保證

**完整檔案**：[`02_event_handler.launch.py`](code/my_cpp_pkg/launch/02_event_handler.launch.py)

```python
from launch.actions import RegisterEventHandler, LogInfo
from launch.event_handlers import OnProcessStart, OnProcessExit

return LaunchDescription([
    turtlesim,

    # turtlesim 起來才啟 smart_brake
    RegisterEventHandler(
        OnProcessStart(
            target_action=turtlesim,
            on_start=[
                LogInfo(msg='✅ turtlesim 起來了，啟動 smart_brake_v2'),
                smart_brake,
            ]
        )
    ),

    # smart_brake 結束 → 印警告
    RegisterEventHandler(
        OnProcessExit(
            target_action=smart_brake,
            on_exit=[
                LogInfo(msg='❌ smart_brake_v2 已結束！系統可能不安全'),
            ]
        )
    ),
])
```

**為什麼我們需要 Event Handler 來控制順序？**

ROS 2 的設計哲學是「各節點啟動順序無關緊要，網路會自己尋找彼此連線」，這聽起來很美好，但在實務開發中，我們常常會撞到一些痛苦的邊角情境 (Corner Cases)：
- **早期訊息被丟失 (Early Message Loss)**：如果發布者 (Publisher) 一啟動就開始狂發重要資料（例如一次性的初始化參數），但訂閱者 (Subscriber) 的節點還在載入中，這段時間內的資料就會永遠消失在虛空中。（雖然這可以透過更改 QoS 為 Transient Local 來解決，但這需要改動 C++ 原始碼，有時不夠彈性。）
- **Lifecycle Node 的嚴格順序要求**：我們在 Phase 09 學過，Lifecycle Node 必須先被明確呼叫 `configure` 後，才能被 `activate`。如果不管順序讓兩個指令同時發出，節點狀態機就會大混亂報錯。
- **RViz 畫面一片空白的尷尬**：當你啟動視覺化工具 RViz 時，如果負責廣播機器人骨架座標轉換 (TF) 的 `robot_state_publisher` 還沒準備好發送資料，RViz 就會因為缺乏參考座標而顯示滿江紅的錯誤，或是一片空白，這常常讓新手一頭霧水。

**Event Handler 提供了完美的宣告式解法**——你完全不用改動任何 C++ 或 Python 邏輯碼，只需要在 Launch 腳本層面定義「等 A 節點徹底啟動後，再放行啟動 B 節點」的規則即可。

**驗證輸出**（剛剛測過）：
```
[INFO] [turtlesim_node-1]: process started with pid [1174]
[INFO] [launch.user]: ✅ turtlesim 起來了，啟動 smart_brake_v2  ← 事件觸發
[INFO] [smart_brake_v2-2]: process started with pid [1176]
[smart_brake_v2-2] [INFO] smart_brake_v2 ready (mode=ENABLED)
```

---

### 範例 3：Conditional — 條件啟動

**完整檔案**：[`03_conditional.launch.py`](code/my_cpp_pkg/launch/03_conditional.launch.py)

```python
from launch.conditions import IfCondition, UnlessCondition

debug = LaunchConfiguration('debug')

return LaunchDescription([
    DeclareLaunchArgument('debug', default_value='false'),

    Node(package='turtlesim', executable='turtlesim_node'),  # 一律啟
    Node(package='phase08_pkg', executable='smart_brake_v2'),  # 一律啟

    # 只有 debug=true 才啟 rqt_graph
    Node(
        package='rqt_graph', executable='rqt_graph', name='debug_graph',
        condition=IfCondition(debug),
    ),
])
```

執行：
```bash
ros2 launch phase11_pkg 03_conditional.launch.py                # 一般模式
ros2 launch phase11_pkg 03_conditional.launch.py debug:=true    # 開 debug
```

**業界最真實的 Conditional 應用場景**：
- **`sim:=true / false` (虛實一鍵切換)**：開發者可以在同一個 Launch 裡，把「真車硬體驅動」與「Gazebo 模擬器」的節點都寫進去。當設定為 `true` 時只啟動模擬器與虛擬感測器，設為 `false` 時則啟動真實的馬達與光達。同一套腳本，無縫適應開發環境與實車部署。
- **`record_bag:=true` (自動錄影模式)**：在實車測試時，我們常需要在背景錄製所有 Topic 的資料以供事後分析。透過參數控制，只要加上 `record_bag:=true`，系統就會在所有的控制節點之外，額外幫你掛載啟動 `ros2 bag record` 程序，非常方便。

---

### 範例 4：Namespace — 多機器人系統

**完整檔案**：[`04_groups_namespace.launch.py`](code/my_cpp_pkg/launch/04_groups_namespace.launch.py)

```python
from launch.actions import GroupAction
from launch_ros.actions import PushRosNamespace

return LaunchDescription([
    # ── Robot 1 ──
    GroupAction([
        PushRosNamespace('robot1'),
        Node(package='turtlesim', executable='turtlesim_node'),
        Node(
            package='phase08_pkg', executable='smart_brake_v2',
            remappings=[('cmd_vel', 'turtle1/cmd_vel')],
        ),
    ]),

    # ── Robot 2 ──
    GroupAction([
        PushRosNamespace('robot2'),
        # 同樣的東西，但 namespace 不同
    ]),
])
```

啟動後 `ros2 topic list`：
```
/robot1/turtle1/cmd_vel    ← Robot 1 的速度通道
/robot1/turtle1/pose
/robot1/brake_status        ← Robot 1 的煞車狀態
/robot2/turtle1/cmd_vel    ← Robot 2 完全獨立
/robot2/turtle1/pose
/robot2/brake_status
```

**兩台車 topic 不會混到**——ROS 2 namespace 機制等同 Linux PID namespace 或 Kubernetes namespace。

**業界大量依賴 Namespace 的應用場景**：
- **龐大的倉儲 AGV 車隊**：想像一個亞馬遜物流中心有 50 台無人搬運車。透過 `robot1` 到 `robot50` 的 Namespace，你可以用同一套控制邏輯程式碼，直接啟動 50 份獨立的控制節點，讓每台車乖乖聽從自己的 `/robotX/cmd_vel` 指令，完全不會發生「下指令給 1 號車，結果 50 台車一起往前衝」的慘劇。
- **多代理人 (Multi-Agent) 強化學習模擬**：在 Gazebo 中同時訓練 10 隻機器狗時，必須把每隻狗的感測器資料與關節狀態嚴格隔離在各自的 Namespace 中，否則神經網路的輸入會把 10 隻狗的數據混在一起大亂。
- **高安全性的主副控冗餘架構**：在自動駕駛系統中，為了極致的安全，可能會有主電腦與備援電腦同時運行相同的演算法。透過 Namespace，可以讓備援系統默默在背景運算（例如它產出的指令會被丟到 `/backup/steering_cmd`），直到主系統失效時，再瞬間無縫接管控制權。

---

## 🚀 Demo 流程

### Step 1：部署 + build

#### ☁️ TheConstructSim
```bash
cd ~/ros2_ws/src
git clone https://github.com/gino07172002/ros2-learning-notes.git
ln -s ros2-learning-notes/phase-11-launch-files-advanced/code/my_cpp_pkg phase11_pkg
```

#### 💻 本機 WSL2
```bash
cp -r /mnt/d/ros_learn/ros2-learning-notes/phase-11-launch-files-advanced/code/my_cpp_pkg \
      ~/ros2_ws/src/phase11_pkg
sed -i 's|<name>my_cpp_pkg</name>|<name>phase11_pkg</name>|' ~/ros2_ws/src/phase11_pkg/package.xml
sed -i 's|project(my_cpp_pkg)|project(phase11_pkg)|' ~/ros2_ws/src/phase11_pkg/CMakeLists.txt
```

```bash
cd ~/ros2_ws
colcon build --packages-select phase11_pkg
source install/setup.bash
```

### Step 2：依序試四個範例

```bash
# 1) Include 嵌套
ros2 launch phase11_pkg 01_include.launch.py

# 2) Event Handler — 看順序保證
ros2 launch phase11_pkg 02_event_handler.launch.py

# 3) Conditional — 試開關
ros2 launch phase11_pkg 03_conditional.launch.py
ros2 launch phase11_pkg 03_conditional.launch.py debug:=true

# 4) Namespace — 兩台車獨立
ros2 launch phase11_pkg 04_groups_namespace.launch.py
# 另開 terminal 看：
ros2 topic list                     # 全部都有 /robot1/ 或 /robot2/ 前綴
ros2 node list                      # 兩組獨立節點
```

---

## 🐛 常見雷

### 雷 1：IncludeLaunchDescription 找不到檔案
路徑用 `os.path.join(get_package_share_directory('其他套件'), 'launch', 'xxx.launch.py')`——**目標套件必須先 build**。

### 雷 2：event_handler 對 lambda 失效
```python
# ❌ on_start 收到的是 list，不是 callable
RegisterEventHandler(OnProcessStart(
    target_action=turtle, on_start=lambda: print("hi")))

# ✅ 用 list of actions
RegisterEventHandler(OnProcessStart(
    target_action=turtle, on_start=[LogInfo(msg='hi'), other_node]))
```

### 雷 3：IfCondition 拿到 string `'false'` 是 truthy
```python
# ❌ 直接比較會錯（LaunchConfiguration 是 lazy object）
if LaunchConfiguration('debug') == 'true': ...

# ✅ 用 IfCondition + LaunchConfiguration
condition=IfCondition(LaunchConfiguration('debug'))
```

`IfCondition('true')` / `IfCondition('false')` / `IfCondition('1')` / `IfCondition('0')` 都正確識別。

### 雷 4：PushRosNamespace 內 remap 用絕對路徑
```python
GroupAction([
    PushRosNamespace('robot1'),
    Node(remappings=[('cmd_vel', '/turtle1/cmd_vel')]),  # ❌ 絕對路徑跳出 namespace
    Node(remappings=[('cmd_vel', 'turtle1/cmd_vel')]),   # ✅ 相對路徑會被 namespace 包進來
])
```

絕對路徑（開頭 `/`）會逃出 namespace、相對路徑會被加上 `/robot1/` 前綴。

### 雷 5：嵌套 launch 內的 args 無法傳入
```python
# 子 launch 接受 obstacle_distance arg，但這樣寫 args 沒傳進去
IncludeLaunchDescription(PythonLaunchDescriptionSource(...))

# ✅ 必須用 launch_arguments 傳
IncludeLaunchDescription(
    PythonLaunchDescriptionSource(...),
    launch_arguments={'obstacle_distance': '0.3'}.items(),
)
```

### 雷 6：event_handler 監控的 process 必須在同一份 launch
你不能用 event_handler 監控「**之前** 用別的 launch 啟動的 Node」。

---

## 🎯 學到的關鍵概念

- **嵌套**：`IncludeLaunchDescription + PythonLaunchDescriptionSource`
- **事件**：`OnProcessStart / OnProcessExit / OnProcessIO`
- **條件**：`IfCondition / UnlessCondition + LaunchConfiguration`
- **隔離**：`GroupAction + PushRosNamespace`
- 這四個機制**組合**使用就是業界 launch 的全部——Nav2 / MoveIt 不過如此

---

## 🌟 進階挑戰

1. **真實 bringup**：寫一個 `bringup.launch.py` include 三個子 launch（perception/control/visualization），每個子 launch 各管一個小子系統
2. **事件鏈**：A 起來 → 觸發 B → B 起來 → 觸發 C
3. **失敗自動重啟**：用 `OnProcessExit` 監控 smart_brake_v2，crash 時重新啟動（小心無限重啟迴圈）
4. **5 台機器人**：用 Python for 迴圈生成 5 個 GroupAction，每台 robot 都有完整堆疊

---

## 👣 下一步去哪？

- [Phase 09 — Executors / Lifecycle / Composition](../phase-09-executors-lifecycle-composition/)：Callback 怎麼被排程、Node 生命週期、多 Node 同 process

---

## 📁 完整檔案結構

```
phase-11-launch-files-advanced/
├── README.md
└── code/
    └── my_cpp_pkg/
        ├── package.xml
        ├── CMakeLists.txt
        └── launch/
            ├── 01_include.launch.py
            ├── 02_event_handler.launch.py
            ├── 03_conditional.launch.py
            └── 04_groups_namespace.launch.py
```
