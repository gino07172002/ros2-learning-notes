# Phase 09：Executors / Lifecycle / Composition

> Part 3 最重的一章。三個觀念合在一起講，因為它們都是「**Node 怎麼被執行**」的內部機制。

**學完你會**：理解 callback 怎麼被排程（為什麼預設單執行緒）、自己選擇 SingleThreaded vs MultiThreaded Executor、寫 LifecycleNode 控制啟動/停止流程、把多個 Node 組進同一個 process（rclcpp_components）。

**前置**：
- [Phase 04 Service](../phase-04-services-toggle/) 與 [Phase 07 Mini Capstone](../phase-07-mini-capstone-1/) — 你寫過多 callback Node、用過 `std::atomic` 防 race

**產出**：
- [`src/executors_demo.cpp`](code/my_cpp_pkg/src/executors_demo.cpp) — 比較 Single/Multi Executor
- [`src/lifecycle_demo.cpp`](code/my_cpp_pkg/src/lifecycle_demo.cpp) — Lifecycle Node 完整生命週期
- [`src/composable_publisher.cpp`](code/my_cpp_pkg/src/composable_publisher.cpp) + [`src/composable_subscriber.cpp`](code/my_cpp_pkg/src/composable_subscriber.cpp) — 可組合 Node
- [`launch/composition_demo.launch.py`](code/my_cpp_pkg/launch/composition_demo.launch.py) — 多 Node 同 process

**環境**：☁️ TheConstructSim + 💻 本機 WSL 雙環境通用。

---

## 📚 閱讀路徑指南(Part 3 最重的一章,別硬塞)

這章塞了三個獨立大觀念,**不要試圖一次消化完**。建議分兩三次來:

| 你現在的目標 | 該讀哪段 | 預計時間 |
|------------|---------|---------|
| 只想跑通 **Phase 14 Capstone 1** | **只讀 Part 2 LifecycleNode**(下面)即可,Capstone 1 主要用這個 | 1 hr |
| 想知道為什麼自己的 multi-callback Node 會卡 | 讀 Part 1 Executors + Part 2 LifecycleNode | 2 hr |
| 想學完整章(進 Part 4 之前最佳狀態) | Part 1 + Part 2 + Part 3 全讀,中間休息 | 3–4 hr,可分兩次 |
| 已會 ROS 1 想快速了解 ROS 2 差異 | 直接看 Part 2 LifecycleNode + Part 3 Composition(這兩個 ROS 1 沒有) | 1.5 hr |

> 💡 **為什麼三個塞一章**:它們都回答同一個問題 — 「**Node 怎麼被執行**」。但三個之間是**獨立的**,先學一個用熟,卡到時再回來補另一個 OK。

> ⚠️ **新手特別注意**:Part 1 的 `std::atomic` / Reentrant CallbackGroup race condition 部分,如果第一次讀看不太懂,**跳過去先讀 Part 2 LifecycleNode**,Capstone 1 不會用到 race condition。等你之後做多 callback 真的踩雷了再回來,會懂得快。

---

## 為什麼三個放一起講

它們都回答同一個問題：「**ROS Node 內部到底怎麼運作？**」

| 概念 | 回答的問題 |
|------|----------|
| Executor | callback 怎麼被排程（thread model） |
| LifecycleNode | Node 啟動/停止的階段控制 |
| Composition | 多個 Node 怎麼共用一個 process |

學完這章你會懂為什麼業界 ROS code 充滿 `std::atomic`、為什麼 Nav2 用一堆 LifecycleNode、為什麼 MoveIt 把一切塞進一個 component_container。

---

## Part 1: Executors — Callback 排程

### `rclcpp::spin()` 內部到底是什麼

你之前一直寫 `rclcpp::spin(node);`。這行做的事：

```cpp
// rclcpp::spin 內部簡化版
auto executor = SingleThreadedExecutor();
executor.add_node(node);
executor.spin();   // 阻塞，跑事件迴圈
```

**Executor 是事件迴圈的調度器**。它把所有 callback（timer、subscription、service、action）放進佇列，然後**用一個 thread 一個一個跑**。

### SingleThreadedExecutor 的問題

```cpp
class TimerDemoNode : public rclcpp::Node {
    rclcpp::TimerBase::SharedPtr slow_timer_;   // 5s 一次, sleep 3s
    rclcpp::TimerBase::SharedPtr fast_timer_;   // 100ms 一次
};
```

預期：`fast_timer` 應該每 100ms 觸發一次。

**實際（SingleThreadedExecutor）**：
```
[FAST] tick=10           ← 1 秒 = 10 tick
[FAST] tick=20
[FAST] tick=30
[FAST] tick=40
[SLOW] start (will sleep 3s)
                          ← FAST 完全靜默 3 秒！
[SLOW] done
```

**為什麼**：SingleThreadedExecutor 只有一個 thread。SLOW callback 跑時，FAST callback 在佇列裡等，沒人去執行。

### 解法：MultiThreadedExecutor + Reentrant CallbackGroup

```cpp
// 1. 改用 MultiThreadedExecutor
rclcpp::executors::MultiThreadedExecutor executor;
executor.add_node(node);
executor.spin();
```

但**只改這個還不夠**——因為預設 callback group 是 **MutuallyExclusive**（同 Node 的所有 callback 互斥）。要改成 **Reentrant**：

```cpp
auto reentrant_group = create_callback_group(
    rclcpp::CallbackGroupType::Reentrant);

slow_timer_ = create_wall_timer(5s, slow_cb, reentrant_group);
fast_timer_ = create_wall_timer(100ms, fast_cb, reentrant_group);
```

**實際（MultiThreadedExecutor + Reentrant）**：
```
[FAST] tick=10
[FAST] tick=20
[FAST] tick=30
[FAST] tick=40
[SLOW] start (will sleep 3s)
[FAST] tick=50           ← 跟 SLOW 並行！
[FAST] tick=60
[SLOW] done
```

🎯 **FAST 不再被 SLOW 卡**——兩個 callback 跑在不同 thread。

### CallbackGroup 三種類型

| 類型 | 行為 |
|------|------|
| **MutuallyExclusive**（預設） | 同 group 內的 callback 互斥，序列執行 |
| **Reentrant** | 同 group 內的 callback 可並行 |
| (Custom) | 自訂優先級、QoS deadline |

### 為什麼預設是 MutuallyExclusive

避免你不小心寫出 race condition。如果你的 callback 共用狀態而沒加 mutex/atomic，預設行為保證安全。**升級到 MultiThreaded 之前，先確認所有共享狀態都用 `std::atomic` 或 mutex 保護**——這就是為什麼 Phase 07 我堅持用 `std::atomic<bool> brake_enabled_`。

---

## Part 2: LifecycleNode — 受控的啟動/停止

### 一般 Node vs LifecycleNode

```cpp
// 一般 Node
class MyNode : public rclcpp::Node {
    MyNode() : Node("my_node") {
        publisher_ = create_publisher<...>(...);  // 立刻可用
        timer_ = create_wall_timer(1s, ...);      // 立刻開始發送
    }
};
```

**問題**：建構子內就開始發送，但訂閱者可能還沒 subscribe → 早期訊息丟失。

```cpp
// LifecycleNode
class MyLifecycleNode : public rclcpp_lifecycle::LifecycleNode {
    // 建構子只做基本初始化，不發送任何訊息

    LifecycleCallbackReturn on_configure(...) override {
        publisher_ = create_publisher<...>(...);  // 建立 publisher
        return SUCCESS;
    }

    LifecycleCallbackReturn on_activate(...) override {
        publisher_->on_activate();                 // 開始能發送
        timer_ = create_wall_timer(1s, ...);       // 啟動 timer
        return SUCCESS;
    }

    LifecycleCallbackReturn on_deactivate(...) override {
        timer_.reset();                            // 停 timer
        publisher_->on_deactivate();
        return SUCCESS;
    }
};
```

### 狀態機

```
       ┌─────────────────────┐
       │   unconfigured       │ ← 出生
       └──────┬───────────────┘
              │ configure()
              ▼
       ┌─────────────────────┐
       │   inactive          │ ← 設定好但不工作
       └──┬───────────────┬──┘
       activate()      cleanup()
          │               │
          ▼               ▼
       ┌──────┐    ┌──────────┐
       │active│    │unconfigured│
       └──┬───┘    └──────────┘
   deactivate()
          │
          ▼
        inactive

   shutdown() 從任何狀態 → finalized
```

### 業界用途

- **Nav2 全部 Node 是 LifecycleNode**：bringup 流程用 nav2_lifecycle_manager 控制 20+ 個 Node 的 configure → activate 順序
- **MoveIt 部分用**：planning_scene_monitor 等核心元件
- **故障隔離**：某個 Node 進 ErrorProcessing 不影響其他 Node

### 控制 LifecycleNode

```bash
# Terminal 1: 啟動 Node
ros2 run phase09_pkg lifecycle_demo

# Terminal 2: 從 CLI 操控狀態切換
ros2 lifecycle list /lifecycle_demo_node           # 看當前狀態
ros2 lifecycle set /lifecycle_demo_node configure
ros2 lifecycle set /lifecycle_demo_node activate
ros2 lifecycle set /lifecycle_demo_node deactivate
ros2 lifecycle set /lifecycle_demo_node cleanup
```

實測 log（**真的跑過**）：
```
Constructed (state: unconfigured)
→ on_configure: declare publisher           ← 收到 configure 命令
→ on_activate: start timer + publisher      ← 收到 activate 命令
[ACTIVE] publishing: I am ALIVE at ...      ← active 期間每秒發
[ACTIVE] publishing: I am ALIVE at ...
→ on_deactivate: stop timer                 ← 收到 deactivate 命令
                                             ← 不再發送
```

### LifecyclePublisher 特殊規定

```cpp
// 跟一般 publisher 不同，必須手動 activate
publisher_->on_activate();    // active 才能 publish
publisher_->on_deactivate();  // inactive 不能 publish
```

漏寫 `on_activate()` 是新手雷 —— publisher 物件存在但呼叫 `publish()` 不會發任何東西。

---

## Part 3: Composition — 多 Node 同 process

### 為什麼要這樣做

你之前每個 Node 一個 `ros2 run`：
```bash
ros2 run pkg node_a    # process 1
ros2 run pkg node_b    # process 2
ros2 run pkg node_c    # process 3
```

每個訊息從 A → B 都要：**序列化 → 跨 process IPC → 反序列化**。

如果 A/B/C 是緊耦合的（例如 perception pipeline），合理做法：**塞進同一個 process**。

```bash
ros2 run rclcpp_components component_container  # 一個 container process
ros2 component load /my_container pkg pkg::NodeA  # 在 container 內載入 A
ros2 component load /my_container pkg pkg::NodeB  # 載入 B
```

**好處**：
- 同 process Node 之間用 **intra-process communication**（直接記憶體共享，零拷貝）
- 啟動快（不用啟多個 process）
- 共用 Executor（能精細排程）

### Nav2 / MoveIt 的實況

`ros2 launch nav2_bringup nav2.launch.py` 啟動後：
- **看 ps -ef**：只有 1–2 個 process
- **看 ros2 node list**：20+ 個 Node

它們全部塞在 `nav2_container` 裡。

### 寫一個可組合 Node

```cpp
#include "rclcpp_components/register_node_macro.hpp"

class ComposablePublisher : public rclcpp::Node {
public:
    // ⚠️ 必須接受 NodeOptions 參數
    explicit ComposablePublisher(const rclcpp::NodeOptions & options)
    : Node("composable_publisher", options) { ... }
};

// 註冊本 class 給 component_container 找得到
RCLCPP_COMPONENTS_REGISTER_NODE(phase09_components::ComposablePublisher)
```

CMakeLists.txt：
```cmake
add_library(my_node SHARED src/composable_publisher.cpp)  # 編成 .so
ament_target_dependencies(my_node rclcpp rclcpp_components ...)
rclcpp_components_register_nodes(my_node "phase09_components::ComposablePublisher")
```

⚠️ 與一般 executable 不同的兩點：
1. 編成 **shared library** (`.so`)，不是 executable
2. 必須用 `RCLCPP_COMPONENTS_REGISTER_NODE` 巨集註冊
3. 建構子簽章是 `(NodeOptions &)`，不是無參數

### 用 Launch File 載入

```python
ComposableNodeContainer(
    name='phase09_container',
    package='rclcpp_components',
    executable='component_container',
    composable_node_descriptions=[
        ComposableNode(
            package='phase09_pkg',
            plugin='phase09_components::ComposablePublisher',  # ← 完整 class 路徑
            name='publisher_node',
        ),
        ComposableNode(
            package='phase09_pkg',
            plugin='phase09_components::ComposableSubscriber',
            name='subscriber_node',
        ),
    ],
)
```

實測（**真的跑過**）：
```
[component_container-1] [INFO] Loaded node '/publisher_node' in container '/phase09_container'
[component_container-1] [INFO] Loaded node '/subscriber_node' in container '/phase09_container'
[component_container-1] [INFO] [publisher_node]: Publishing: Hello from publisher #0
[component_container-1] [INFO] [subscriber_node]: Received: Hello from publisher #0
```

`ps -ef | grep container` 只看到一個 process，但 `ros2 node list` 看到兩個 Node。

---

## 🚀 Demo 流程

### Step 1：部署 + build

#### ☁️ TheConstructSim
```bash
cd ~/ros2_ws/src
git clone https://github.com/gino07172002/ros2-learning-notes.git
ln -s ros2-learning-notes/phase-09-executors-lifecycle-composition/code/my_cpp_pkg phase09_pkg
```

#### 💻 本機 WSL2
```bash
cp -r /mnt/d/ros_learn/ros2-learning-notes/phase-09-executors-lifecycle-composition/code/my_cpp_pkg \
      ~/ros2_ws/src/phase09_pkg
sed -i 's|<name>my_cpp_pkg</name>|<name>phase09_pkg</name>|' ~/ros2_ws/src/phase09_pkg/package.xml
sed -i 's|project(my_cpp_pkg)|project(phase09_pkg)|' ~/ros2_ws/src/phase09_pkg/CMakeLists.txt
```

```bash
cd ~/ros2_ws
colcon build --packages-select phase09_pkg
source install/setup.bash
```

### Step 2：Demo 1 — Executors 對比

```bash
# Single — 觀察 SLOW 卡住 FAST
ros2 run phase09_pkg executors_demo single
# 跑 8 秒看 log，5 秒處 SLOW 開始 sleep 3 秒，期間 FAST 完全靜默

# Multi — 觀察兩個並行
ros2 run phase09_pkg executors_demo multi
# 跑 8 秒看 log，5 秒處 SLOW 開始，但 FAST 仍然繼續每秒 +10
```

### Step 3：Demo 2 — Lifecycle

Terminal 1：
```bash
ros2 run phase09_pkg lifecycle_demo
```

Terminal 2：
```bash
ros2 lifecycle list /lifecycle_demo_node       # 應該顯示 unconfigured
ros2 lifecycle set /lifecycle_demo_node configure
# Terminal 1 出現 → on_configure: declare publisher

ros2 lifecycle set /lifecycle_demo_node activate
# Terminal 1 出現 → on_activate + 開始每秒 publish "I am ALIVE"

ros2 topic echo /heartbeat
# 看到 String 訊息流動

ros2 lifecycle set /lifecycle_demo_node deactivate
# Terminal 1 停止 publish
```

### Step 4：Demo 3 — Composition

```bash
ros2 launch phase09_pkg composition_demo.launch.py
```

預期看到：
```
Loaded node '/publisher_node' in container '/phase09_container'
Loaded node '/subscriber_node' in container '/phase09_container'
[publisher_node]: Publishing: Hello from publisher #0
[subscriber_node]: Received: Hello from publisher #0
```

驗證只有一個 process：
```bash
ps -ef | grep component_container    # 只有一個 process

ros2 node list                        # 兩個 Node
# /phase09_container
# /publisher_node
# /subscriber_node
```

---

## 🐛 常見雷

### 雷 1：MultiThreadedExecutor 但 callback 還是序列
**原因**：CallbackGroup 預設 MutuallyExclusive。**必須明確建立 Reentrant group**並指定給 callback。

### 雷 2：升級 Multi 之後 race condition crash
**原因**：共享狀態沒保護。**用 `std::atomic` 或 `std::mutex`**。Phase 07 的 `is_brake_active_` 改 atomic 就是這個原因。

### 雷 3：LifecyclePublisher 漏 `on_activate`
```cpp
// active 之後，必須手動 activate publisher
publisher_->on_activate();
```
漏寫 publisher 不會發任何訊息（也不報錯）。

### 雷 4：Composable Node 建構子漏 NodeOptions
```cpp
// ❌
explicit MyNode() : Node("my_node") { }

// ✅
explicit MyNode(const rclcpp::NodeOptions & options)
    : Node("my_node", options) { }
```
component_container 載入時會傳 options 進來。

### 雷 5：CMakeLists 把 composable lib install 到 lib/${PROJECT_NAME}
```cmake
# ❌ 一般 executable 才用這個
install(TARGETS my_lib DESTINATION lib/${PROJECT_NAME})

# ✅ Composable lib 安裝到 lib (不含 ${PROJECT_NAME})
install(TARGETS my_lib
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin)
```

### 雷 6：plugin 名稱錯
```python
ComposableNode(
    package='phase09_pkg',
    plugin='phase09_components::ComposablePublisher',  # ✅ 完整 namespace
    plugin='ComposablePublisher',                       # ❌ 找不到
)
```

---

## 🎯 學到的關鍵概念

- **`rclcpp::spin(node)` 內部就是 SingleThreadedExecutor**
- **MultiThreadedExecutor + Reentrant CallbackGroup** 才能真正並行
- **共享狀態防護**：升級 Multi 之前先 atomic / mutex
- **LifecycleNode 五狀態**：unconfigured → inactive → active → inactive → cleanup → finalized
- **LifecyclePublisher 必須 `on_activate()`**
- **Composition 把多 Node 塞進一個 process**：用 shared library + RCLCPP_COMPONENTS_REGISTER_NODE
- **業界 Nav2 / MoveIt 全部用這套**：LifecycleNode + Composition

---

## 🌟 進階挑戰

1. **多執行緒 thread 數限制**：`MultiThreadedExecutor executor(rclcpp::ExecutorOptions(), 4)` — 只給 4 thread。試試看 thread 用完會怎樣
2. **Lifecycle 鏈式啟動**：寫一個 lifecycle_manager Node，等 all 子 Node 都 configured 才一起 activate
3. **Composition 加 lifecycle**：把 LifecycleNode 也做成可組合 Node
4. **Intra-process zero-copy**：開 Composition + 訂閱大訊息（PointCloud2），用 `intra_process_comms=true`，比較 CPU 使用率

---

## 下一步

- [Phase 12 — 測試（gtest + launch_testing）](../phase-12-testing/)（待完成）
- [Phase 13 — Actions 進階](../phase-13-actions-advanced/)（待完成）

---

## 📁 完整檔案結構

```
phase-09-executors-lifecycle-composition/
├── README.md
└── code/
    └── my_cpp_pkg/
        ├── package.xml
        ├── CMakeLists.txt
        ├── src/
        │   ├── executors_demo.cpp           ← Single vs Multi 對比
        │   ├── lifecycle_demo.cpp           ← 完整生命週期
        │   ├── composable_publisher.cpp     ← 可組合 Pub
        │   └── composable_subscriber.cpp    ← 可組合 Sub
        └── launch/
            └── composition_demo.launch.py   ← 載入兩個 Composable 到同一 container
```
