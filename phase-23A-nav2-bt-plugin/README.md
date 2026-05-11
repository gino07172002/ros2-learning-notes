# Phase 23A:自訂 Nav2 Behavior Tree plugin

> 寫一個 C++ BT condition node `IsBatteryLow`,訂 `/battery_state`,當電量低於閾值回 SUCCESS。**Nav2 自訂行為的標準入口** — 業界擴充 Nav2 8 成都從寫 BT plugin 開始。

**學完你會**:
- 寫 BT.cpp v3 的 condition node(繼承 `BT::ConditionNode`,實作 `tick()`)
- 用 `BT_REGISTER_NODES` 巨集把 plugin 註冊給 BT factory
- 用 `BT::InputPort` 讓 BT XML 可以傳參數進來
- 從 BT blackboard 取出 nav2 的 `rclcpp::Node::SharedPtr` 做 ROS pub/sub
- 寫 gtest 整合測試:**load plugin → 餵 ROS 訊息 → 驗 tick 結果**(4 個 case 全過)

**前置**:
- [Phase 19 pluginlib](../phase-19-pluginlib/) — runtime 載 C++ class 的觀念
- [Phase 22A Nav2 入門](../phase-22A-nav2-basics/) — 知道 BT 在 Nav2 內的角色
- [Phase 12 Testing](../phase-12-testing/) — gtest + ament 測試

**產出**:
- [`include/my_bt_plugin/is_battery_low_condition.hpp`](code/my_bt_plugin/include/my_bt_plugin/is_battery_low_condition.hpp)
- [`src/is_battery_low_condition.cpp`](code/my_bt_plugin/src/is_battery_low_condition.cpp)
- [`test/test_load_plugin.cpp`](code/my_bt_plugin/test/test_load_plugin.cpp) — 4 個 gtest case
- 編譯成 `libis_battery_low_condition_bt_node.so`

**環境**:☁️💻 雙環境通用(純 C++ 編譯 + gtest,不需 Gazebo / GPU)

> **完整 WSL 驗證**(不像 Phase 21A/22A 受 GPU 限制):colcon build + colcon test 全過 ✅

---

## 🤔 為什麼這章重要

**Nav2 的 bt_navigator 是用 BT.cpp(BehaviorTree.CPP)當決策引擎**。Nav2 預設 BT 規劃失敗 → spin → backup → 再規劃,這串行為就是 BT XML 描述的。

業界要客製 Nav2 行為(電量低自動充電、看到人停下來、特定區域減速),**不是改 Nav2 source code**,而是**寫一個 BT plugin** runtime 載入。

寫 BT plugin 是 Nav2 ecosystem 的「準入門檻」 — 會寫 = 你可以做業界 8 成的 Nav2 客製化需求。

---

## 🏗️ BT plugin 架構

```
┌────────────────────────────────────────────────────────────┐
│                BT.cpp BehaviorTree.CPP                     │
│                                                            │
│  BT XML (navigate_to_pose.xml):                            │
│  ┌──────────────────────────────────────────────────┐      │
│  │ <Sequence>                                       │      │
│  │   <Condition ID="IsBatteryLow"                  │  ◄── 我們寫的
│  │              battery_topic="/battery_state"     │      │
│  │              min_battery="0.2"/>                │      │
│  │   <Action ID="ComputePathToPose"                │      │
│  │           goal="${dock_pose}"/>                 │      │
│  │   <Action ID="FollowPath"/>                     │      │
│  │ </Sequence>                                     │      │
│  └──────────────────────────────────────────────────┘      │
│                                                            │
│  Plugin: libis_battery_low_condition_bt_node.so           │
│    └─ class IsBatteryLowCondition : ConditionNode          │
│       └─ tick() {                                          │
│            if (battery < threshold) return SUCCESS;        │
│            else return FAILURE;                            │
│          }                                                 │
└────────────────────────────────────────────────────────────┘
                          │
                          │ BT_REGISTER_NODES 巨集
                          ▼
                  Nav2 bt_navigator
                  (runtime dlopen .so)
```

`bt_navigator` 看到 XML 內 `<IsBatteryLow ...>`,去查 plugin registry,找到我們的 `.so` 載入,呼叫 `tick()`。**不用改 nav2 source 一行字**。

---

## 💻 重點檔案

### 1. include/my_bt_plugin/is_battery_low_condition.hpp

```cpp
class IsBatteryLowCondition : public BT::ConditionNode
{
public:
  IsBatteryLowCondition(const std::string & condition_name,
                        const BT::NodeConfiguration & conf);

  BT::NodeStatus tick() override;     // 每次 BT 走到這 node,呼叫 tick

  // 對外宣告 input ports — BT XML 可傳參數進來
  static BT::PortsList providedPorts() {
    return {
      BT::InputPort<std::string>("battery_topic", "/battery_state",
                                 "Topic name for battery state"),
      BT::InputPort<double>("min_battery", 0.2,
                            "Battery percentage threshold (0–1)"),
    };
  }

private:
  rclcpp::Node::SharedPtr node_;     // 從 blackboard 拿到 nav2 的 node
  rclcpp::Subscription<sensor_msgs::msg::BatteryState>::SharedPtr battery_sub_;
  double battery_percentage_ = 1.0;
  double min_battery_ = 0.2;
  bool received_first_msg_ = false;
};
```

**亮點**:Condition node 跟 Action node 不一樣 —
| 種類 | tick 回傳 |
|------|----------|
| Condition | 立刻回 SUCCESS / FAILURE |
| Action | 可能回 RUNNING(下次再 tick),長任務用 |

### 2. src/is_battery_low_condition.cpp

```cpp
IsBatteryLowCondition::IsBatteryLowCondition(...)
  : BT::ConditionNode(condition_name, conf)
{
  // 從 BT blackboard 拿 ROS node(nav2 bt_navigator 會塞進來)
  node_ = config().blackboard->get<rclcpp::Node::SharedPtr>("node");

  getInput("min_battery", min_battery_);
  getInput("battery_topic", battery_topic_);

  battery_sub_ = node_->create_subscription<sensor_msgs::msg::BatteryState>(
    battery_topic_, rclcpp::SystemDefaultsQoS(),
    std::bind(&IsBatteryLowCondition::batteryCallback, this, _1));
}

BT::NodeStatus IsBatteryLowCondition::tick()
{
  if (!received_first_msg_) return BT::NodeStatus::FAILURE;
  if (battery_percentage_ < min_battery_) return BT::NodeStatus::SUCCESS;
  return BT::NodeStatus::FAILURE;
}

// === 必要的 plugin 註冊巨集 ===
BT_REGISTER_NODES(factory)
{
  factory.registerNodeType<my_bt_plugin::IsBatteryLowCondition>("IsBatteryLow");
}
```

**`BT_REGISTER_NODES`** 是關鍵 — 沒這個 nav2 dlopen 後找不到 entry point,plugin 載入失敗。

### 3. CMakeLists.txt — 編成 SHARED library

```cmake
add_library(is_battery_low_condition_bt_node SHARED
  src/is_battery_low_condition.cpp)

ament_target_dependencies(is_battery_low_condition_bt_node
  rclcpp behaviortree_cpp_v3 nav2_behavior_tree sensor_msgs)

install(TARGETS is_battery_low_condition_bt_node
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib)
```

**SHARED 不能改 STATIC** — Nav2 runtime dlopen,STATIC 不能 dlopen。

### 4. test/test_load_plugin.cpp — 4 個 gtest case

```cpp
TEST_F(IsBatteryLowFixture, NoMessageReturnsFailure)    { ... }
TEST_F(IsBatteryLowFixture, FullBatteryReturnsFailure)  { ... }
TEST_F(IsBatteryLowFixture, LowBatteryReturnsSuccess)   { ... }
TEST_F(IsBatteryLowFixture, ExactThresholdReturnsFailure) { ... }
```

每個 case:
1. `BehaviorTreeFactory::registerNodeType<...>` 註冊 plugin
2. 從 inline XML(`<root><BehaviorTree><Sequence><IsBatteryLow .../></Sequence>...`)創 tree
3. Publish `BatteryState` 給 ROS topic
4. spin_some 讓 sub 收到
5. `tree.tickRoot()` 看回 SUCCESS / FAILURE 對不對

**這套測試 100% 在 WSL 跑得過**,不需 nav2 / Gazebo / GPU。

---

## 🚀 完整 Demo 流程(WSL,驗證過)

### Step 1:部署 + build

```bash
rm -rf ~/ros2_ws/src/my_bt_plugin
cp -r /mnt/d/ros_learn/ros2-learning-notes/phase-23A-nav2-bt-plugin/code/my_bt_plugin \
      ~/ros2_ws/src/my_bt_plugin
source /opt/ros/humble/setup.bash
cd ~/ros2_ws && colcon build --packages-select my_bt_plugin
```

驗證過輸出:
```
Starting >>> my_bt_plugin
Finished <<< my_bt_plugin [40.0s]
Summary: 1 package finished [40.4s]
```

### Step 2:跑測試

```bash
colcon test --packages-select my_bt_plugin
colcon test-result --test-result-base build/my_bt_plugin --verbose
```

**驗證過輸出**:
```
Summary: 5 tests, 0 errors, 0 failures, 0 skipped
```

5 個 test = 4 個 gtest case + 1 個 ament_lint check 都過 ✅

### Step 3:看 plugin .so 真的有產出

```bash
ls ~/ros2_ws/install/my_bt_plugin/lib/libis_battery_low_condition_bt_node.so
```

驗證過存在,大小約 90KB(C++ shared library)。

### Step 4:接到真實 Nav2(Phase 22A 上層,雲端 / 實機跑)

啟動 Nav2 stack 時,把我們的 plugin 加到 bt_navigator 的 `plugin_lib_names`:

```yaml
bt_navigator:
  ros__parameters:
    plugin_lib_names:
      - is_battery_low_condition_bt_node    # ← 加這行
      # 預設的 node 也保留
      - nav2_compute_path_to_pose_action_bt_node
      - ... (其他預設)
```

寫一個自訂 BT XML(`my_navigate_with_charging.xml`):

```xml
<root main_tree_to_execute="MainTree">
  <BehaviorTree ID="MainTree">
    <Fallback>
      <!-- 電量低 → 去充電(覆蓋一般 navigate 行為) -->
      <Sequence>
        <IsBatteryLow battery_topic="/battery_state" min_battery="0.2"/>
        <ComputePathToPose goal="0;0;0" path="{path}"/>
        <FollowPath path="{path}"/>
      </Sequence>
      <!-- 否則正常 navigate -->
      <RecoveryNode number_of_retries="6" name="NavigateRecovery">
        <ComputePathToPose .../>
        <FollowPath path="{path}"/>
      </RecoveryNode>
    </Fallback>
  </BehaviorTree>
</root>
```

然後 nav2 params 指向這個 XML:
```yaml
bt_navigator:
  ros__parameters:
    default_nav_to_pose_bt_xml: /path/to/my_navigate_with_charging.xml
```

啟動 Nav2、發 `/battery_state` percentage=0.1,看車自己往充電站跑。

---

## 🐛 常見雷

### ⚠️ 雷 1:漏 `BT_REGISTER_NODES` 巨集,plugin 載入失敗

**症狀**:nav2_bt_navigator 啟動 log 出現:
```
[bt_navigator]: Failed to dynamic_cast plugin from libxxx.so → could not find symbol BT_RegisterPlugin
```

**原因**:BT.cpp 的 plugin loader 用 `dlsym` 找 `BT_RegisterPlugin` symbol。這個 symbol 由 `BT_REGISTER_NODES` 巨集自動產生,**沒這個巨集就完全載不了**。

**解**:每個 BT plugin .cpp 結尾必加:
```cpp
#include "behaviortree_cpp_v3/bt_factory.h"
BT_REGISTER_NODES(factory)
{
  factory.registerNodeType<my_bt_plugin::IsBatteryLowCondition>("IsBatteryLow");
}
```

### ⚠️ 雷 2:CMakeLists 寫 STATIC,nav2 載不了

**症狀**:`add_library(... STATIC ...)`,build 過,但 nav2 啟動 `Failed to dynamic_cast plugin`。

**原因**:Nav2 用 `dlopen()` runtime 載入 .so,**STATIC 出來是 .a,不能 dlopen**。

**解**:`add_library(... SHARED ...)`,輸出 .so。

### ⚠️ 雷 3:從 blackboard 拿 node 時 segfault

**症狀**:plugin 載入成功、tick 第一次就崩潰。

**原因**:`config().blackboard->get<rclcpp::Node::SharedPtr>("node")` 在 ctor 內呼叫,但**「node」key 在 nav2 bt_navigator 才會被填**。如果你獨立用 BT.cpp 跑 plugin(沒透過 nav2),blackboard 沒這個 key → 拿到 null → 用它呼叫 method 就崩。

**解**:測試時自己 `blackboard->set<rclcpp::Node::SharedPtr>("node", node_)`(本章 test_load_plugin.cpp 有做)。生產時 nav2 自動處理。

### ⚠️ 雷 4:Subscription 在 ConditionNode ctor 創,但 tick 時 callback 沒進

**症狀**:battery 訊息發出去,但 plugin 內 `received_first_msg_` 永遠 false。

**原因**:**ConditionNode tick 是同步呼叫**,callback 跑在另一個 executor。如果 nav2 的 executor 沒輪到 spin 你的 sub,callback 就沒進。

**解**:
1. 確認 nav2 bt_navigator 的 executor 能 spin 我們的 sub(它本來就會)
2. 測試時要 `rclcpp::spin_some()` 才會觸發 callback(本章 test 有做)

### ⚠️ 雷 5:Input port 的型別跟 XML 內傳的字串不對應

**症狀**:啟動 nav2 看到 `BehaviorTree XML loading error: parameter not convertible`。

**原因**:BT XML 的 attribute 是 string,要 BT.cpp 自動轉成你 portsList 宣告的型別。如果你 `min_battery="abc"` 傳給 `InputPort<double>`,BT.cpp 不認得 → 解析失敗。

**解**:XML 用合理數值(`min_battery="0.2"`),且 portsList 的型別是 `double` / `int` / `std::string`,別用太奇特的。

### ⚠️ 雷 6:用 BT.cpp v4 寫 Nav2 plugin,plugin 載不了

**症狀**:用 BT.cpp v4 (`behaviortree_cpp` 沒 `_v3` 後綴)的 API 寫,build 過但 nav2 載入失敗。

**原因**:**ROS 2 Humble 的 nav2 還用 BT.cpp v3,API 跟 v4 不完全相容**(v4 改了 ports declaration、改了 blackboard API)。混用會 ABI 不相容。

**解**:Humble 配 BT.cpp v3:
- package.xml:`<depend>behaviortree_cpp_v3</depend>`(不是 `behaviortree_cpp`)
- code:`#include "behaviortree_cpp_v3/condition_node.h"`(有 `_v3`)

ROS 2 Iron 之後升 v4,Humble 階段先別跨。

---

## 🎯 學到的關鍵概念

| 概念 | 一句話 |
|------|------|
| BT.cpp ConditionNode vs ActionNode | Condition 立刻回 SUCCESS/FAILURE,Action 可回 RUNNING |
| `BT_REGISTER_NODES` 巨集 | 必要,提供 dlopen 的 entry point |
| `providedPorts()` | 讓 BT XML 傳參數,類似 ROS parameter |
| Blackboard 共享 ROS node | nav2 自動塞 `node` key,讓 plugin pub/sub |
| `add_library(... SHARED ...)` | nav2 dlopen 必需 SHARED |
| BT.cpp v3 vs v4 | Humble 鎖 v3,別跨版本 |

---

## 🌟 進階挑戰

1. **寫 ActionNode**:`GoToChargingStation` action(回 RUNNING 直到車到充電站),比 Condition 複雜
2. **寫 Decorator**:`OnceEvery5Sec`(限制底下子節點 5 秒只 tick 一次),BT 工具箱常用
3. **複雜 BT**:組 `IsBatteryLow → SetBlackboard(dock_pose) → ComputePathToPose → FollowPath` 完整充電行為
4. **寫 launch_test**:不只 unit test,還測 plugin 真的被 nav2 載入
5. **發 Nav2 plugin**:做出泛用 plugin 像 `nav2_human_avoidance_bt_node`,push 到 GitHub

---

## 🔗 下一步

- **[Phase 22A Nav2](../phase-22A-nav2-basics/)** — 在 nav2_params.yaml 內加 `plugin_lib_names: ["is_battery_low_condition_bt_node"]` 真的用起來
- **Capstone A** — 整合 SLAM + Nav2 + 自訂 BT,完整 demo
- **[Phase 19 pluginlib](../phase-19-pluginlib/)** — 對照 ros2 pluginlib 跟 BT.cpp plugin 機制差異

---

## 📁 完整檔案結構

```
phase-23A-nav2-bt-plugin/
├── README.md
├── code/
│   └── my_bt_plugin/
│       ├── package.xml
│       ├── CMakeLists.txt
│       ├── include/
│       │   └── my_bt_plugin/
│       │       └── is_battery_low_condition.hpp
│       ├── src/
│       │   └── is_battery_low_condition.cpp
│       └── test/
│           └── test_load_plugin.cpp           ← 4 個 gtest case
└── images/                                    ← (之後補:nav2 載入 plugin 截圖)
```
