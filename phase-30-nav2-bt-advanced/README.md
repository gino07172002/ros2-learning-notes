# Phase 30:Nav2 Behavior Tree 進階 — 完整 plugin 集

> 從 [Phase 23A](../phase-23A-nav2-bt-plugin/) 1 個 condition node 升級到 **4 個自訂 BT node + 完整 BT XML**,涵蓋 BT.cpp 的全部 4 個 base class。寫一個「**電量低自動充電,否則巡邏 3 個 waypoint**」的完整充電/巡邏行為樹。

**學完你會**:
- 寫 `StatefulActionNode`(`onStart` / `onRunning` / `onHalted` 三 lifecycle method)
- 寫 `DecoratorNode`(`tick()` 控制底下子節點要不要跑)
- 用 BT XML 把 `Fallback` / `Sequence` 跟自訂 node 組成完整行為樹
- 用 `BT::OutputPort` 把 plan 結果寫進 blackboard 給其他 node 用
- 寫 6 個 gtest case 全部驗證 RUNNING / SUCCESS / FAILURE 三態

**前置**:
- [Phase 23A Nav2 BT plugin](../phase-23A-nav2-bt-plugin/) — ConditionNode 基礎,本章升級到 ActionNode + DecoratorNode
- [Phase 22A Nav2](../phase-22A-nav2-basics/) — bt_navigator 載 plugin 的場景
- [Phase 12 Testing](../phase-12-testing/) — gtest

**產出**:
- [`include/my_bt_advanced/*.hpp`](code/my_bt_advanced/include/my_bt_advanced/) — 4 個 BT node header
- [`src/*.cpp`](code/my_bt_advanced/src/) — 實作 + plugin registration
- [`trees/patrol_with_charging.xml`](code/my_bt_advanced/trees/patrol_with_charging.xml) — 完整充電/巡邏 BT
- [`test/test_advanced_nodes.cpp`](code/my_bt_advanced/test/test_advanced_nodes.cpp) — 6 個 gtest case

**環境**:☁️💻 雙環境通用(純 C++ 編譯 + gtest,不需 GPU / Gazebo)

> ⚠️ **狀態說明**:Code 寫完但寫作當下 WSL 系統暫時卡住(systemd-user-session 抽風,連 `wsl --` 命令都掛起),**未即時跑 `colcon build` 跟 `colcon test`**。Code 結構跟 [Phase 23A](../phase-23A-nav2-bt-plugin/) 同模式(Phase 23A 有 5 tests, 0 errors 驗證紀錄),預期 build + test 都能過。WSL 恢復後會補上實測 log。

---

## 為什麼這章重要

業界 Nav2 客製化幾乎都從**寫一堆 BT plugin** 開始。Phase 23A 已經演示過 1 個 ConditionNode,但**真實 Nav2 應用要 5–10 個自訂 node**:

- ActionNode(送 nav2 goal、開抓爪、播音樂)
- DecoratorNode(節流、retry、condition wrapper)
- 整個 BT XML(Sequence / Fallback / SubTree 重用)

寫過這些**才算「會 Nav2 BT」**,不只「看過 BT.cpp tutorial」。

---

## 🏗️ 4 種 BT node 對照

```
BT.cpp 的 4 個 base class:

┌──────────────────────────┐   ┌──────────────────────────┐
│ ConditionNode            │   │ SimpleActionNode         │
│ tick() 立刻回 SUCCESS / FAIL │   │ tick() 立刻完成的動作   │
│ Phase 23A 的 IsBatteryLow│   │ (啟發版,本章沒寫)        │
└──────────────────────────┘   └──────────────────────────┘

┌──────────────────────────┐   ┌──────────────────────────┐
│ StatefulActionNode       │   │ DecoratorNode            │
│ 跨多個 tick 的長任務:     │   │ 包覆一個子節點,改變行為:│
│   onStart / onRunning /  │   │   tick() 內呼 child_node_│
│   onHalted               │   │     ->executeTick()      │
│ ★ 本章 GoToCharger /     │   │ ★ 本章 OnceEvery /        │
│   PatrolWaypoint         │   │   CountSuccesses          │
└──────────────────────────┘   └──────────────────────────┘
```

每個 base class 解決不同問題:Action 處理「需要時間的事」,Decorator 處理「對子節點加邏輯」。

---

## 💻 4 個 BT node 詳解

### 1. `GoToCharger` — StatefulActionNode

**長任務範例**:模擬「往充電站走一段時間」,跨多個 tick 才完成。

```cpp
class GoToChargerAction : public BT::StatefulActionNode {
public:
  BT::NodeStatus onStart() override;     // 第 1 次 tick — 初始化計時
  BT::NodeStatus onRunning() override;   // 後續 tick — 看時間到沒,RUNNING / SUCCESS
  void onHalted() override;              // 樹被中止 — cancel goal 收尾

  static BT::PortsList providedPorts() {
    return {
      BT::InputPort<double>("travel_time", 2.0, "Simulated travel time in seconds"),
      BT::OutputPort<std::string>("result_msg", "Last result text"),
    };
  }
};
```

**lifecycle method 對照**:
| Method | 何時呼叫 | 回什麼 |
|--------|---------|-------|
| `onStart()` | 第 1 次 tick | RUNNING / SUCCESS / FAILURE |
| `onRunning()` | 第 2 次以後的 tick | RUNNING(還沒完)/ SUCCESS / FAILURE |
| `onHalted()` | 父節點 abort 我 | void(只清乾淨) |

**業界對應**:nav2 `NavigateToPose` action client 就是這結構 — onStart 送 goal,onRunning 等 result,onHalted 呼 cancel_goal。

### 2. `OnceEvery` — DecoratorNode(時間節流)

**功能**:每 N 秒才 tick 一次底下子節點,其他時間直接回 FAILURE。

```cpp
BT::NodeStatus OnceEveryDecorator::tick() {
  double interval = 5.0;
  getInput("seconds", interval);

  auto now = steady_clock::now();
  auto elapsed = duration_cast<duration<double>>(now - last_tick_).count();

  if (first_call_ || elapsed >= interval) {
    first_call_ = false;
    last_tick_ = now;
    return child_node_->executeTick();   // 讓子節點真的跑
  }
  return BT::NodeStatus::FAILURE;        // 還在 interval 內,跳過
}
```

**業界用例**:
- 限制 action 重執行頻率(避免 spam nav2)
- 「每 30 秒巡邏一次」「每 5 分鐘自我診斷」

### 3. `CountSuccesses` — DecoratorNode(累計次數)

**功能**:子節點累計 N 次 SUCCESS 後,decorator 才回 SUCCESS。失敗就 reset。

```cpp
BT::NodeStatus CountSuccessesDecorator::tick() {
  auto child_status = child_node_->executeTick();
  if (child_status == BT::NodeStatus::RUNNING)  return BT::NodeStatus::RUNNING;
  if (child_status == BT::NodeStatus::FAILURE) {
    success_count_ = 0;
    return BT::NodeStatus::FAILURE;
  }
  // SUCCESS:
  success_count_++;
  if (success_count_ >= target) {
    success_count_ = 0;
    return BT::NodeStatus::SUCCESS;
  }
  return BT::NodeStatus::RUNNING;       // 還沒達到目標
}
```

**跟內建 `RepeatNode` 不一樣**:
- `RepeatNode`:**強制重複** N 次,不管子是否成功
- `CountSuccesses`:**累計 N 次成功**,失敗 reset(更嚴格)

### 4. `PatrolWaypoint` — StatefulActionNode(維護內部狀態)

**功能**:巡邏到下個 waypoint,內部維護 `current_idx_`,每次 SUCCESS 後 +1,wrap-around 回 0。

亮點:**用 `OutputPort<int>` 把當前 waypoint index 寫進 blackboard**,讓其他 BT node(例如「印 log」「發 visualization marker」)可以拿到:

```cpp
BT::NodeStatus PatrolWaypointAction::onRunning() {
  if (時間到) {
    setOutput<int>("current_waypoint", current_idx_);    // ← 寫 blackboard
    current_idx_ = (current_idx_ + 1) % total_waypoints_;
    return BT::NodeStatus::SUCCESS;
  }
  return BT::NodeStatus::RUNNING;
}
```

---

## 🌳 完整 BT XML — 充電/巡邏

完整見 [`trees/patrol_with_charging.xml`](code/my_bt_advanced/trees/patrol_with_charging.xml)。

```xml
<root main_tree_to_execute="MainTree">
  <BehaviorTree ID="MainTree">

    <Fallback name="charge_or_patrol">

      <!-- 任務 1:電量低就去充電 -->
      <Sequence name="charge_sequence">
        <IsBatteryLow battery_topic="/battery_state" min_battery="0.2"/>
        <GoToCharger travel_time="3.0" result_msg="{charge_result}"/>
      </Sequence>

      <!-- 任務 2:巡邏(每 3 秒走一個 waypoint,連走 3 個算一輪) -->
      <CountSuccesses target_count="3">
        <OnceEvery seconds="3.0">
          <PatrolWaypoint
            total_waypoints="3"
            travel_time="1.0"
            current_waypoint="{wp_idx}"/>
        </OnceEvery>
      </CountSuccesses>

    </Fallback>

  </BehaviorTree>
</root>
```

**讀法**:
1. `Fallback`(?)從上往下試 child,**第一個非 FAILURE 就 return**:
2. **電量低 + 充電完成** → 第 1 條 Sequence 走完 SUCCESS → Fallback 整個 SUCCESS
3. **電量正常** → IsBatteryLow FAILURE → 跳第 2 條,進巡邏:
4. `CountSuccesses(3)` 包覆 → 等 3 次 SUCCESS 才整體 SUCCESS
5. `OnceEvery(3s)` 包覆 → 限制每 3 秒才 tick 一次 PatrolWaypoint
6. `PatrolWaypoint` 走完一個 waypoint → SUCCESS 一次 → 進下一輪節流等待 3 秒

**展示了 BT 的精華**:每個 node 只做一件事,組合產生複雜行為。

---

## 🚀 完整 Demo 流程(WSL,純 C++ + gtest)

### Step 1:部署 + build

```bash
rm -rf ~/ros2_ws/src/my_bt_advanced
cp -r /mnt/d/ros_learn/ros2-learning-notes/phase-30-nav2-bt-advanced/code/my_bt_advanced \
      ~/ros2_ws/src/my_bt_advanced
source /opt/ros/humble/setup.bash
cd ~/ros2_ws && colcon build --packages-select my_bt_advanced
```

### Step 2:跑 6 個 gtest case

```bash
colcon test --packages-select my_bt_advanced
colcon test-result --test-result-base build/my_bt_advanced --verbose
```

**6 個測試 case**(寫作當下 WSL 系統卡住未跑,結構照 Phase 23A 同模式,預期全過):
- `GoToChargerTest.RunningThenSuccess` — RUNNING → 等到時間 → SUCCESS
- `OnceEveryTest.ThrottleBetweenIntervals` — interval 內 FAILURE,過了再 SUCCESS
- `CountSuccessesTest.AccumulatesUntilTarget` — 連 3 次 SUCCESS 才整體 SUCCESS
- `CountSuccessesTest.ChildFailureResetsCounter` — 子失敗整體立刻 FAILURE
- `PatrolWaypointTest.IndexAdvancesAndWraps` — index 0→1→2→0 wrap-around
- `IntegrationTest.ChargingPathPicksWhenChildSucceeds` — Fallback 第一個成功就停

### Step 3:確認 .so 真的有產出

```bash
ls ~/ros2_ws/install/my_bt_advanced/lib/libmy_bt_advanced_nodes.so
```

可以塞給 nav2_bt_navigator 載入(配 `plugin_lib_names`)。

---

## 🐛 常見雷

### ⚠️ 雷 1:StatefulActionNode 第一個 tick 就回 SUCCESS / FAILURE 而不是 RUNNING

**症狀**:長任務 demo 裡寫
```cpp
BT::NodeStatus onStart() override {
  if (already_done()) return BT::NodeStatus::SUCCESS;   // ❌
  start_timer();
  return BT::NodeStatus::RUNNING;
}
```
看似合理,但 `Fallback` 內如果這個 child 第一個 tick 就 SUCCESS,**不會走進 onRunning**。

**解**:除非真的「秒完成」,**onStart 一律回 RUNNING**,把判斷邏輯放 onRunning。

### ⚠️ 雷 2:DecoratorNode 忘了傳 RUNNING 透傳給父節點

**症狀**:子節點正在跑(RUNNING),但 decorator 自己回 FAILURE / 0,父節點以為子失敗了。

**原因**:Decorator `tick()` 必須**處理子節點 RUNNING 的情況**:

```cpp
auto child_status = child_node_->executeTick();
if (child_status == BT::NodeStatus::RUNNING) {
  return BT::NodeStatus::RUNNING;     // ← 必傳
}
// ... 處理 SUCCESS / FAILURE
```

**解**:本章 `CountSuccesses::tick()` 第一行就先 handle RUNNING,展示這個模式。

### ⚠️ 雷 3:OutputPort 寫 blackboard 但讀不到 — 拼錯名字

**症狀**:`PatrolWaypoint` 寫 `current_waypoint`,XML 寫 `{wp_idx}`,但讀 blackboard `wp_idx` 永遠拿不到值。

**原因**:在 `setOutput<int>("current_waypoint", ...)` 時,`current_waypoint` 是 **port 名**,XML 內 `current_waypoint="{wp_idx}"` 是把 port 結果**對映到 blackboard key `wp_idx`**。

**解**:讀的時候用 `wp_idx`,不是 `current_waypoint`。**port 名跟 blackboard key 是兩個東西**,XML 是橋樑。

### ⚠️ 雷 4:多個 BT plugin 註冊在一個 .so 內,但 BT_REGISTER_NODES 只能寫一次

**症狀**:把 4 個 `factory.registerNodeType<...>` 拆成 4 個 `BT_REGISTER_NODES`,只有最後一個生效。

**原因**:`BT_REGISTER_NODES` 巨集底下展開成一個 `BT_RegisterPlugin` symbol,**多次定義會被 linker 取最後一個**。

**解**:**所有 registerNodeType 寫在同一個 BT_REGISTER_NODES 巨集內**(本章 `plugin_registration.cpp` 就是這結構)。

### ⚠️ 雷 5:onHalted 漏掉清理 → resource leak

**症狀**:plugin 接 nav2 NavigateToPose,父節點 abort 後再起,前次的 goal 還在跑。

**原因**:`onHalted` 是 BT.cpp 給你**清乾淨的最後機會**:cancel action goal、reset state、close file 等。沒做這些,下次 onStart 啟動時拿到的是髒狀態。

**解**:每個 StatefulActionNode 的 `onHalted` 都明確列出清理項目。

### ⚠️ 雷 6:gtest 的 RUNNING tick 沒等就立刻檢查 → 永遠 fail

**症狀**:
```cpp
EXPECT_EQ(tree.tickRoot(), BT::NodeStatus::SUCCESS);   // ❌ 第 1 個 tick 是 RUNNING
```

**解**:測 StatefulActionNode 要 `while (status == RUNNING) sleep_for + tick`,本章 `PatrolWaypointTest` 就是這樣寫。

---

## 🎯 學到的關鍵概念

| 概念 | 一句話 |
|------|------|
| `StatefulActionNode` 三 lifecycle | onStart 第 1 次,onRunning 後續,onHalted 中止 |
| `DecoratorNode` `tick()` 包子節點 | `child_node_->executeTick()` 是核心 |
| RUNNING 必傳父 | 子 RUNNING 時 decorator 不能私自決定 SUCCESS/FAIL |
| OutputPort 寫 blackboard | port 名 ≠ blackboard key,XML 是橋樑 |
| `BT_REGISTER_NODES` 一個 .so 一次 | 多 node 也是一次寫完所有 registerNodeType |
| `onHalted` 必清理 | nav2 cancel_goal、reset state、close handle |

---

## 🌟 進階挑戰

1. **真的接 nav2**:把 GoToCharger 改成 `rclcpp_action::Client<NavigateToPose>` 真送 goal、cancel goal,實機可用
2. **SubTree 重用**:寫 `<SubTree ID="ChargingSubTree"/>`,把充電邏輯抽成可重用單元
3. **Behavior Tree Editor (Groot)**:用 GUI 拖拉編輯 XML,匯出後直接給 nav2_bt_navigator
4. **接到 [Capstone A](../phase-CapstoneA-mobile/)**:把 auto_navigator 換成 BT 驅動,改 nav2_params 加 `plugin_lib_names: ["my_bt_advanced_nodes"]`
5. **接 perception**:寫 `IsHumanInFront` ConditionNode 訂 vision msg,組合 BT「看到人就停」

---

## 🔗 下一步

- **[Phase 23A Nav2 BT plugin](../phase-23A-nav2-bt-plugin/)** — 對照看 ConditionNode 跟本章 ActionNode/DecoratorNode 差異
- **[Phase 22A Nav2 入門](../phase-22A-nav2-basics/)** — 把這些 plugin 塞進 nav2_bt_navigator
- **[Capstone A](../phase-CapstoneA-mobile/)** — 用 BT 取代 hardcode auto_navigator

---

## 📁 完整檔案結構

```
phase-30-nav2-bt-advanced/
├── README.md
├── code/
│   └── my_bt_advanced/
│       ├── package.xml
│       ├── CMakeLists.txt
│       ├── include/
│       │   └── my_bt_advanced/
│       │       ├── go_to_charger_action.hpp        ← StatefulActionNode
│       │       ├── once_every_decorator.hpp        ← DecoratorNode 節流
│       │       ├── count_successes_decorator.hpp   ← DecoratorNode 累計
│       │       └── patrol_waypoint_action.hpp      ← StatefulActionNode + OutputPort
│       ├── src/
│       │   ├── *.cpp                                ← 4 個實作
│       │   └── plugin_registration.cpp             ← BT_REGISTER_NODES 一次註冊全部
│       ├── trees/
│       │   └── patrol_with_charging.xml            ← 完整充電/巡邏 BT
│       └── test/
│           └── test_advanced_nodes.cpp             ← 6 個 gtest case
└── images/                                         ← (之後補:Groot 視覺化截圖)
```
