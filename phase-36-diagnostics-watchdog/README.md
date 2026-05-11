# Phase 36:Diagnostics + Heartbeat Watchdog

> 寫一個 watchdog node 監控 N 個 topic 心跳,沒收到就標 ERROR,把整體健康狀態用 `diagnostic_updater` 自動發到 `/diagnostics`,再用 `diagnostic_aggregator` 階層聚合。**業界 oncall paging / 健康檢查的標準做法**,純 C++ 內建套件,WSL2 + ROS 2 Humble 一行裝好。

**學完你會**:
- 用 `diagnostic_updater::Updater` 自動每秒發 `/diagnostics`(不用自己寫 timer)
- 寫 watchdog 邏輯:多 topic 心跳監控、timeout → STALE → ERROR 三段
- 用 `diagnostic_aggregator` 把零散的 status 按 group 聚合(Sensors / Drive / System)
- 把 watchdog 邏輯抽成 library 給 main + gtest 共用(4 個 gtest case)
- 看懂 `OK / WARN / ERROR / STALE` 4 等級在 Foxglove / rqt_robot_monitor 怎麼顯示

**前置**:
- [Phase 09 Lifecycle](../phase-09-executors-lifecycle-composition/) — 健康監控通常綁 lifecycle 狀態
- [Phase 12 Testing](../phase-12-testing/) — gtest fixture 觀念
- [Phase 23A BT plugin](../phase-23A-nav2-bt-plugin/) — library + gtest 同模式

**產出**:
- [`include/my_diag_demo/heartbeat_watchdog.hpp`](code/my_diag_demo/include/my_diag_demo/heartbeat_watchdog.hpp)
- [`src/heartbeat_watchdog.cpp`](code/my_diag_demo/src/heartbeat_watchdog.cpp) — 核心 watchdog 邏輯
- [`src/watchdog_main.cpp`](code/my_diag_demo/src/watchdog_main.cpp) — main 入口
- [`src/fake_heartbeater.cpp`](code/my_diag_demo/src/fake_heartbeater.cpp) — 測試用心跳產生器(可設「跑 N 秒就死」)
- [`config/aggregator.yaml`](code/my_diag_demo/config/aggregator.yaml) — 階層聚合設定
- [`launch/watchdog_demo.launch.py`](code/my_diag_demo/launch/watchdog_demo.launch.py) — 一鍵 demo(5 秒後 imu 假裝掛掉)
- [`test/test_heartbeat_watchdog.cpp`](code/my_diag_demo/test/test_heartbeat_watchdog.cpp) — 4 個 gtest case

**環境**:☁️💻 雙環境通用,純 C++ + ROS 2 內建 `diagnostic_*`,不需 Gazebo / GPU

---

## 🤔 為什麼這章重要

業界 ROS 2 系統一旦上線(實機 / 工廠 / 客戶現場),最常被問的兩件事:

1. **「現在好不好?」** — Sensors 都活著嗎?馬達熱嗎?剩多少電?
2. **「壞了怎麼通知?」** — 誰幫你 paging?要看 Foxglove 還是 PagerDuty?

ROS 2 內建的解法是 **diagnostic_updater + diagnostic_aggregator**:
- 每個 node 把自己的健康狀態用 `Updater` 發到 `/diagnostics`(自動定時 + summary 機制)
- aggregator 把零散訊息聚成階層(`/Sensors/Lidar`、`/Sensors/IMU`、`/Drive/MotorL`)
- Foxglove / rqt_robot_monitor 直接訂 `/diagnostics_agg` 變成階層樹狀圖
- 上層接 PagerDuty / Slack webhook(自己加 bridge node 訂 `/diagnostics_agg`)

這章把最常見的「**心跳監控**」場景寫一次:給 watchdog 一張 topic 清單,沒看到該 topic 的心跳就降級,有看到就維持綠燈。

> 業界對照:Nav2 內 [`bt_navigator` 自帶 diagnostic publisher](https://github.com/ros-navigation/navigation2/tree/main/nav2_bt_navigator);MoveIt servo 也用 `Updater` 發 servo 健康狀態。**Diagnostics 是 ROS 2 健康監控的事實標準,不是「選讀」。**

---

## 🗺️ 全圖

```
┌─────────────────────────────────────────────────────────────────┐
│                    /lidar_hb (std_msgs/Empty)                   │
│   fake_heartbeater ───────► [永遠跳,200ms/次]                   │
│                                                                  │
│   fake_heartbeater ───────► /imu_hb [跑 5 秒就死]                │
└────────────────────────────┬────────────────────────────────────┘
                             │ subscribe
                             ▼
              ┌──────────────────────────────┐
              │   heartbeat_watchdog          │
              │   - last_seen_[topic] = now   │ ◄── on each msg
              │                                │
              │   每秒 produce_diagnostic():    │
              │     ∀ topic: age = now - last  │
              │     age > timeout → STALE      │
              │     never seen   → ERROR       │
              │     summary(OK/WARN/ERROR)     │
              └──────────────┬────────────────┘
                             │ publishes
                             ▼
                    /diagnostics (zero-conf)
                             │
                             ▼ subscribed by
              ┌──────────────────────────────┐
              │   diagnostic_aggregator        │
              │   yaml: Sensors / System group │
              └──────────────┬────────────────┘
                             ▼
                    /diagnostics_agg (階層化)
                             │
                             ▼
                    Foxglove / rqt_robot_monitor / 自家 paging bridge
```

---

## 🛠️ Step-by-step

### Step 1:用 Updater 取代手寫 timer

最差的做法是自己寫 timer 每秒組 `DiagnosticArray` 訊息發出去 — 容易忘 timestamp、忘 hardware_id、忘 KV 細節格式。

**正確做法**(`heartbeat_watchdog.cpp` 內):
```cpp
updater_ = std::make_shared<diagnostic_updater::Updater>(this);
updater_->setHardwareID("watchdog_demo");
updater_->add("Heartbeats",
              std::bind(&HeartbeatWatchdog::produce_diagnostic, this, _1));
```

`Updater` 自己:
- 預設每 1Hz 呼叫你的 callback(可改 `~diagnostic_period` 參數)
- 把你 `stat.add(key, value)` 的東西組成 `DiagnosticStatus` 訊息
- 把 `stat.summary(level, msg)` 的等級當 status level
- timestamp / `hardware_id` / `name` 全自動填好

### Step 2:summary 等級的選擇

```cpp
if (missing > 0) stat.summary(ERROR, "...");
else if (stale > 0) stat.summary(WARN, "...");
else stat.summary(OK, "...");
```

| level | 何時用 | rqt 顏色 |
|-------|--------|---------|
| `OK` (0) | 一切正常 | 綠 |
| `WARN` (1) | 有風險但還在跑(stale 1 個 sensor、低電量、CPU 高) | 黃 |
| `ERROR` (2) | 故障(sensor 完全沒回應、馬達 fault) | 紅 |
| `STALE` (3) | **node 自己沒在更新 diagnostics**(整個 watchdog 死了) | 灰 |

注意 `STALE` 是給 aggregator 自動發的 — 你的 callback 一段時間沒被呼叫,aggregator 會自動標 STALE。**不要自己 summary STALE**(除非有特殊需要)。

### Step 3:aggregator 階層聚合

[`config/aggregator.yaml`](code/my_diag_demo/config/aggregator.yaml):
```yaml
analyzers:
  sensors:
    type: diagnostic_aggregator/AnalyzerGroup
    path: Sensors
    analyzers:
      heartbeats:
        type: diagnostic_aggregator/GenericAnalyzer
        contains: ['Heartbeats']
```

aggregator 訂 `/diagnostics`,按 yaml 的 `contains` / `startswith` / `name` 規則,把 status 分到不同 group,組成樹狀。Foxglove `Diagnostics` panel 訂 `/diagnostics_agg` 直接畫成樹。

### Step 4:把 watchdog 邏輯抽成 library

跟 Phase 23A 同模式 — `add_library(heartbeat_watchdog SHARED)`,main 跟 gtest 都 `target_link_libraries(... heartbeat_watchdog)`。

好處:
- gtest 不用啟 ROS executor,直接 new node 呼 `simulate_beat()` / `time_since_last_beat_ns()`
- 之後可以把 watchdog 變 component(rclcpp_components) 不用改邏輯

---

## 🐛 踩到的雷

### 雷 1:Updater 在 constructor 裡 `make_shared<Updater>(this)` 直接炸 `bad_weak_ptr`

**現象**:
```
terminate called after throwing an instance of 'std::bad_weak_ptr'
```

**根因**:`Updater` 內部對 node 呼 `shared_from_this()`,但 Node 的 ctor 還沒結束,那個 weak_ptr 還沒生效。

**解**:**ctor 裡用 `std::make_shared<Updater>(this)` 是 OK 的(它只存 raw ptr 與 interface)** — 但若改成需要 shared_from_this 的 API(例如 LifecycleNode 的 SharedPtrAccessors),要把 Updater 初始化挪到 `on_configure()` 裡或第一個 timer callback 內。

→ 本章 ctor 內直接傳 `this` 是 OK 的(普通 Node + 構造完全結束才會 spin),寫成 LifecycleNode 才要小心。

### 雷 2:`/diagnostics` 看得到、但 `/diagnostics_agg` 永遠空

**現象**:`ros2 topic echo /diagnostics` 看到自己發的訊息,但 aggregator 啟動了,`/diagnostics_agg` 完全沒東西。

**根因**:`aggregator.yaml` 內 `contains` 寫錯名字。aggregator 是用「字串包含」比對 `DiagnosticStatus.name`,你 `updater_->add("Heartbeats", ...)` 的字串 = `"watchdog_demo: Heartbeats"`(自動加 hardware_id 前綴),yaml 內 `contains: ['Heartbeats']` 對得到,但 `contains: ['/lidar_hb']` 永遠對不到。

**驗證**:
```bash
ros2 topic echo /diagnostics --once    # 看 name 欄到底長啥
```

**解**:`contains` 用 status name 的子字串,跟 hardware_id 一起對。

### 雷 3:`stat.summary(0, "OK")` 跟 `stat.summaryf(...)` 行為不一樣

**現象**:寫 `summary(0, "OK: " + std::to_string(n))`,訊息 OK 但組字串有 race condition / 編譯慢。

**根因**:`summary` 第一個參數是 `unsigned char level`(0/1/2/3),第二個是 `std::string message`。`summaryf` 是 printf 風格(`summaryf(0, "OK: %zu", n)`),但裡面用 vsnprintf,對 buffer overflow 安全度比 std::to_string 拼字串差。

**解**:用常數 `DiagnosticStatus::OK` / `WARN` / `ERROR`(語意清楚),訊息直接 `+` 拼好(短字串 std::to_string 就夠)。

### 雷 4:gtest 跑完 hang 不 exit

**現象**:`colcon test` 跑完 4 個 case 都過,但 process 不退出,要 Ctrl+C。

**根因**:每個 TEST 都 `rclcpp::init(...)`,但沒 shutdown,executor / context 還在,某些 timer / sub thread 卡著。

**解**(本章 main 內):
```cpp
int main(int argc, char ** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  int rc = RUN_ALL_TESTS();
  if (rclcpp::ok()) rclcpp::shutdown();
  return rc;
}
```

並且 fixture 的 `SetUp()` 用 `if (!rclcpp::ok()) rclcpp::init(0, nullptr);` 避免重複 init。

### 雷 5:`hb_period_ms = 50` 結果 watchdog 還是常常報 STALE

**現象**:fake_heartbeater 50ms 跳一次,timeout 設 1.0s,還是偶爾跳出 STALE。

**根因**:WSL2 上 timer 精度爛,`/clock` 用 system_clock 加上 SHM 有時候 jitter 會到 200ms+。設 `timeout_sec` 至少要是 `hb_period_ms` 的 5~10 倍,才有空間。

**解**:**timeout 至少抓 5x heartbeat 週期**。實機(沒 WSL)可以縮到 2x。

---

## 🚀 跑起來

```bash
# 部署
cp -r code/my_diag_demo ~/ros2_ws/src/
cd ~/ros2_ws
colcon build --packages-select my_diag_demo
source install/setup.bash

# Demo:5 秒前 OK,5 秒後 imu 假死,整體變 WARN
ros2 launch my_diag_demo watchdog_demo.launch.py
```

預期看到:

```
[lidar_heartbeater] Beating /lidar_hb every 200ms
[imu_heartbeater] Beating /imu_hb every 200ms (will stop)
[watchdog_node] Watching 2 topic(s), timeout=1.00s
... 5 秒過後 ...
[imu_heartbeater] Stopped publishing after 5.0s (simulating death)
```

另開 terminal 看 diagnostics:
```bash
# 原始 status (各 node 自己發)
ros2 topic echo /diagnostics

# 階層化的(aggregator 整理過)
ros2 topic echo /diagnostics_agg

# 直接看單一 watchdog 等級
ros2 topic echo /diagnostics --once | grep -A 1 level
```

5 秒前:
```
level: 0
message: All 2 heartbeats alive
```

5 秒後:
```
level: 1
message: 1 topic(s) stale
values:
  - key: /lidar_hb
    value: ok (0.012s ago)
  - key: /imu_hb
    value: STALE (3.214s ago)
```

跑 gtest:
```bash
cd ~/ros2_ws
colcon test --packages-select my_diag_demo
colcon test-result --test-result-base build/my_diag_demo --verbose
```

預期 4 個 case 全過(NeverSeen / AfterBeat / AgesIncreaseOverTime / MultiTopicIndependent)。

---

## 📦 業界進階用法

寫完這章,下一步業界做的事:

- **接 PagerDuty / Slack** — 寫一個 bridge node 訂 `/diagnostics_agg`,等級 ≥ ERROR 就 POST webhook
- **Foxglove Diagnostics panel** — 直接訂 `/diagnostics_agg`,儀表板紅綠燈
- **CPU / Memory monitor** — `apt install ros-humble-system-status`,加進 watchdog group
- **disk space watchdog** — 自己寫一個 node,`statfs(/)` 拿剩餘空間,< 10% 就 ERROR
- **rosbag record /diagnostics_agg** — 客戶現場錄一週的健康狀態,出問題時回放分析

---

## 🔗 相關章節

- [Phase 09 Lifecycle](../phase-09-executors-lifecycle-composition/) — Lifecycle node 通常每個 transition 都發 diagnostic
- [Phase 12 Testing](../phase-12-testing/) — fixture-based gtest 模式
- [Phase 23A BT plugin](../phase-23A-nav2-bt-plugin/) — library + gtest + plugin registration 同模式
- [Phase 35 Foxglove Bridge](../phase-35-foxglove-bridge/) — Foxglove Studio 看 `/diagnostics_agg` 階層樹,本章 demo 直接接它

---

> **驗證狀態**:✅ **WSL 完整驗證**(2026-05-05)— colcon build 通過、colcon test **4 個 gtest case 全過,0 errors / 0 failures**。詳見 [verify_log.md](../verify_log.md)。
