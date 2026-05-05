# Phase 37:LifecycleNode + Diagnostics 整合

> 把 [Phase 09 LifecycleNode](../phase-09-executors-lifecycle-composition/) 跟 [Phase 36 diagnostic_updater](../phase-36-diagnostics-watchdog/) 縫起來:**LifecycleNode 每個 transition 自動更新自己的健康狀態**,unconfigured/inactive 標 WARN、active 標 OK、有 error 標 ERROR。業界實機 ROS 2 node 的標準骨架,給你抄。

**學完你會**:
- 把 `diagnostic_updater::Updater` 接到 `LifecycleNode` 上(跟普通 `Node` 不一樣 — 有雷)
- 設計 transition 規則:configure 內建 Updater、cleanup 內 reset、active 標 OK
- 用 `MultiThreadedExecutor` 解 lifecycle service callback 跟 work timer 的死鎖
- 寫 fixture-based gtest 直接呼 `node_->configure()` / `activate()`(不必啟 service client)
- 把這個範例當骨架,套到你自己的 production node

**前置**:
- [Phase 09 Executors / Lifecycle / Composition](../phase-09-executors-lifecycle-composition/)
- [Phase 14 Capstone 1 ApproachController](../phase-14-capstone-1/) — Lifecycle 已用
- [Phase 36 Diagnostics + Watchdog](../phase-36-diagnostics-watchdog/) — diagnostic_updater 觀念

**產出**:
- [`include/my_lifecycle_diag/healthy_lifecycle_node.hpp`](code/my_lifecycle_diag/include/my_lifecycle_diag/healthy_lifecycle_node.hpp)
- [`src/healthy_lifecycle_node.cpp`](code/my_lifecycle_diag/src/healthy_lifecycle_node.cpp) — 5 個 transitions + diagnostic
- [`src/healthy_main.cpp`](code/my_lifecycle_diag/src/healthy_main.cpp) — `MultiThreadedExecutor` spin
- [`launch/healthy_demo.launch.py`](code/my_lifecycle_diag/launch/healthy_demo.launch.py) — 自動 transition + aggregator
- [`test/test_healthy_lifecycle.cpp`](code/my_lifecycle_diag/test/test_healthy_lifecycle.cpp) — 5 個 gtest case

**環境**:☁️💻 純 C++ + ROS 2 內建,WSL2 / TheConstructSim 都能跑

---

## 為什麼這章重要

「LifecycleNode 我會」+「diagnostic_updater 我會」!= 「會把它們**正確接起來**」。

實際在實機 ROS 2 系統,我們希望:
- **`unconfigured`** → diagnostic 標 WARN「未啟動」
- **`inactive`**(configured 但沒 activate)→ WARN「閒置中」
- **`active`** → OK「正常工作中,tick=N」
- **`active` 但有 error** → ERROR「Active but had error: ...」
- **`finalized`** → STALE(aggregator 自動處理)

這樣 oncall 在 Foxglove 看到一棵紅綠燈樹,不用看每個 node 自己的 log,**直接知道哪一個 node 壞了、現在是什麼 lifecycle 狀態**。

**這章寫的是骨架,業界產品 node 都長這樣**:
- Nav2 的 `lifecycle_manager` 也是這套
- MoveIt 2 的 servo_node 是
- 任何「需要 graceful start/stop」的服務型 node 都是

---

## 🗺️ 全圖

```
External (lifecycle_manager / launch script / 你 ros2 lifecycle set):
   ros2 lifecycle set /healthy_lifecycle_node configure
                                    │
                                    ▼ service call
   ┌──────────── HealthyLifecycleNode ──────────────────┐
   │                                                     │
   │  on_configure()  ◄── 在這裡 new Updater (不在 ctor) │
   │  on_activate()   ◄── 啟動 work_timer, active_=true  │
   │  on_deactivate() ◄── stop timer, active_=false      │
   │  on_cleanup()    ◄── reset Updater + tick_count_=0  │
   │  on_shutdown()   ◄── 收一切                         │
   │                                                     │
   │  on_work_timer() ── 100ms 跑一次,active 才算 tick   │
   │                                                     │
   │  produce_diagnostic(stat):                          │
   │    summary 等級依 lifecycle state + last_error      │
   │    add lifecycle_state / tick_count / active /      │
   │        last_error 鍵值                              │
   └─────────────────┬───────────────────────────────────┘
                     │ /diagnostics (auto via Updater)
                     ▼
                /diagnostics_agg → Foxglove panel
```

---

## 🛠️ Step-by-step

### Step 1:Updater 不要在 ctor 建,挪到 `on_configure`

普通 `rclcpp::Node` 在 ctor 內 `new Updater(this)` 是 OK 的,**LifecycleNode 不行**。

**雷的細節**:`Updater` 內部呼 node 的 `get_node_topics_interface()` 等介面,LifecycleNode 的這些介面在 ctor 還沒完全建好,直接拿會拿到部分初始化的物件,跑 service call 時當機。

```cpp
// ❌ ctor 內這樣寫,啟動就 segfault
HealthyLifecycleNode::HealthyLifecycleNode(...) {
  updater_ = std::make_shared<diagnostic_updater::Updater>(this);  // 炸
}

// ✅ 挪到 on_configure
CallbackReturn HealthyLifecycleNode::on_configure(const State&) {
  updater_ = std::make_shared<diagnostic_updater::Updater>(this);
  updater_->setHardwareID("healthy_lifecycle_node");
  updater_->add("LifecycleHealth", std::bind(...));
  return CallbackReturn::SUCCESS;
}
```

並且 `on_cleanup` 內 `updater_.reset()` — 讓 cleanup 真的釋放資源,符合 lifecycle 語意。

### Step 2:work timer 只在 active 才算 tick

```cpp
void on_work_timer() {
  if (!active_) return;   // inactive 期不要做事
  simulate_tick();
}
```

`active_` flag 跟 lifecycle state 同步:
- `on_activate`:`active_ = true`
- `on_deactivate`:`active_ = false`
- `on_cleanup` / `on_shutdown`:`active_ = false`

**這是 lifecycle 的關鍵設計** — work_timer 雖然在 `on_activate` 內 create、`on_deactivate` 內 reset,但實機常會「activate 過一次後 deactivate,timer 物件還沒被 GC,callback 已經 queue 在 executor」,如果沒 active flag 兜底,deactivate 後還會跑 1~2 次 callback。

### Step 3:summary 規則

```cpp
if (!last_error_.empty() && active_) {
  stat.summary(ERROR, "Active but had error: " + last_error_);
} else if (active_) {
  stat.summary(OK, "Active, " + std::to_string(tick_count_) + " ticks");
} else if (label == "inactive") {
  stat.summary(WARN, "Configured but not activated");
} else {
  stat.summary(WARN, "Lifecycle state: " + label);
}
```

讓 oncall 看一眼就知道「這個 node 現在哪個狀態 + 健康嗎」,不用翻 log。

### Step 4:`MultiThreadedExecutor` 解 lifecycle service 死鎖

`healthy_main.cpp`:
```cpp
rclcpp::executors::MultiThreadedExecutor exec;
exec.add_node(node->get_node_base_interface());
exec.spin();
```

**為什麼必要**:LifecycleNode 內部有兩個 callback group:
- 預設 group:work_timer 跑這邊
- service group:`/get_state` `/change_state` 等 lifecycle service

單執行緒 executor 下,`ros2 lifecycle set ... activate` 進到 service callback,callback 內呼 `on_activate` create 新 timer — 但這個 timer 想跑 callback 必須等 service callback 退出 → executor thread 一直在 service 沒釋放 → deadlock 數秒到 timeout。

`MultiThreadedExecutor` 預設 thread 數 = `std::thread::hardware_concurrency()`,夠用。

### Step 5:fixture-based gtest 直接呼 transition

```cpp
TEST_F(LifecycleFixture, ActivateStartsTicking) {
  configure();   // 直接呼 node_->configure(),不走 service
  activate();
  node_->simulate_tick();
  EXPECT_EQ(node_->tick_count(), 1u);
}
```

`LifecycleNode::configure()` / `activate()` 是 public API,直接觸發內部 transition、呼到你的 `on_configure` / `on_activate`,**不必啟 ros2 service client**,gtest 跑得超快(< 1 秒跑 5 個 case)。

---

## 🐛 踩到的雷

### 雷 1:Updater 在 LifecycleNode ctor 內 `make_shared<Updater>(this)` 直接 segfault

**現象**:Node 啟動 0.5 秒後 SIGSEGV。

**根因**:見 Step 1 — LifecycleNode 的 NodeBaseInterface 在 ctor 還沒完整。

**解**:挪到 `on_configure`,並在 `on_cleanup` 重置。

### 雷 2:單執行緒 executor 下 `ros2 lifecycle set ... activate` 卡 5 秒 timeout

**現象**:`ros2 lifecycle set /node activate` CLI 卡住 5 秒後 timeout,但 node 內 log 顯示 `on_activate` 已經跑完。

**根因**:見 Step 4 — service callback 跟 timer create 同 group 死鎖,直到 timer 真的 init 完才能釋放 callback。

**解**:`MultiThreadedExecutor`,或把 lifecycle service 放獨立 callback group(複雜,推薦前者)。

### 雷 3:on_deactivate 後 timer 還跑 1~2 次

**現象**:`ros2 lifecycle set ... deactivate` 後,log 又印了一次 `simulate_tick()` 的訊息。

**根因**:executor 的 ready set 已經 queue 了下一次 timer callback,deactivate 期間還沒被取消。

**解**:`active_` flag 在 callback 裡面早早 return:
```cpp
void on_work_timer() {
  if (!active_) return;
  // ...
}
```

### 雷 4:`get_current_state().label()` 在 ctor 期回 `"unknown"`

**現象**:gtest 第一個 case `EXPECT_EQ(label, "unconfigured")` fail,實際拿到 `"unknown"`。

**根因**:`State::label()` 回的是 `lifecycle_msgs/msg/State` 的 `label` 字串,ctor 還沒完全結束時 lifecycle state machine 還沒 init。

**解**:**ctor 完全結束後再讀**(本章 gtest fixture 在 SetUp 內 `make_shared` 完才開始 EXPECT,沒問題)。如果你需要在 ctor 內看狀態,讀 `get_current_state().id()` 跟 `lifecycle_msgs::msg::State::PRIMARY_STATE_*` 比較數字。

### 雷 5:`launch_ros.events.lifecycle.ChangeState + EmitEvent` API 在 Humble 不穩

**現象**:用標準 launch event API 觸發 lifecycle transition,有時候 transition 沒被觸發、有時候觸發了但 state 沒更新。

**根因**:Humble 上 `launch_ros` 的 lifecycle event handler 跟 `EmitEvent` 路徑有 timing race,而且 doc 半新半舊。

**解**:**用 `ExecuteProcess(['ros2', 'lifecycle', 'set', ...])`** 這條 CLI 路徑最穩定,本章 launch 用的就是這個。

---

## 🚀 跑起來

```bash
cp -r code/my_lifecycle_diag ~/ros2_ws/src/
cd ~/ros2_ws
colcon build --packages-select my_lifecycle_diag
source install/setup.bash

# Demo:auto-transition unconfigured → configure (2s) → activate (4s)
ros2 launch my_lifecycle_diag healthy_demo.launch.py
```

預期 log:
```
[healthy_lifecycle_node] ctor: state=unconfigured
[ros2 lifecycle set...] Transitioning successful  ← 2s 後
[healthy_lifecycle_node] on_configure
[ros2 lifecycle set...] Transitioning successful  ← 4s 後
[healthy_lifecycle_node] on_activate
```

另開 terminal:
```bash
# 看單一狀態快照(active 後 tick_count 持續增長)
ros2 topic echo /diagnostics --once

# 看到的應該是:
# level: 0 (OK)
# message: Active, 47 ticks
# values:
#   - key: lifecycle_state
#     value: active
#   - key: tick_count
#     value: '47'
#   - key: active
#     value: yes
```

跑 gtest:
```bash
colcon test --packages-select my_lifecycle_diag
colcon test-result --test-result-base build/my_lifecycle_diag --verbose
```

預期 5 個 case 全過。

---

## 📦 業界進階用法

寫完這章你可以這樣套到自己的 production node:

1. **每個 production node 都繼承 LifecycleNode** + 寫 `Updater`(本章模板)
2. **lifecycle_manager** 統一管理一組 node 的啟動順序(Nav2 內就是這樣管 8 個 node)
3. **Foxglove Diagnostics panel** 直接看樹狀,oncall 一眼看出哪個壞
4. **錯誤注入測試**:gtest 用 `inject_error` 模擬各種 sensor lost / IO failure,**驗證 diagnostic 等級會升級成 ERROR**

---

## 🔗 相關章節

- [Phase 09 Executors / Lifecycle](../phase-09-executors-lifecycle-composition/) — Lifecycle 入門
- [Phase 14 Capstone 1](../phase-14-capstone-1/) — Lifecycle + Action 整合
- [Phase 36 Diagnostics + Watchdog](../phase-36-diagnostics-watchdog/) — diagnostic_updater 入門
- [Phase 35 Foxglove Bridge](../phase-35-foxglove-bridge/) — 看本章的 `/diagnostics` 樹

---

> **驗證狀態**:✅ **WSL 完整驗證**(2026-05-05)— 首輪抓到 [Bug 1](../verify_log.md#bug-1-phase-37--get_current_state-不是-constwrapper-不能標-const)(`get_current_state()` 不是 const,wrapper 不能標 const,已修;header 加註解警示)。修完後 colcon build 通過、colcon test **5 個 gtest case 全過,0 errors / 0 failures**。詳見 [verify_log.md](../verify_log.md)。
