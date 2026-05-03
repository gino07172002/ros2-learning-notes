# Phase 13：Actions 進階

**學完你會**：寫 Action Server 處理 cancel / abort / reject 三種「非正常結束」、寫 Action Client 中途取消、理解業界 Nav2/MoveIt 的 action 介面為什麼這樣設計。

**前置**：
- [Phase 08 Custom Interfaces](../phase-08-custom-interfaces/) — 包含本章用的 Countdown.action interface

**產出**：
- [`src/countdown_server.cpp`](code/my_cpp_pkg/src/countdown_server.cpp) — 完整 cancel/abort/reject 邏輯
- [`src/countdown_client.cpp`](code/my_cpp_pkg/src/countdown_client.cpp) — 可用 SIGINT 取消的 client

**環境**：☁️ TheConstructSim + 💻 本機 WSL 雙環境通用。

---

## 為什麼進階

Phase 08 你寫了 Approach action server，但只處理「正常完成」一種情況。實務上 action 有 **5 種結束方式**：

| 結束方式 | 觸發者 | 用 callback |
|---------|-------|-----------|
| **REJECT** | Server 在 handle_goal 拒絕 | `return GoalResponse::REJECT` |
| **ACCEPT_AND_EXECUTE** | Server 接受並開始執行 | `return GoalResponse::ACCEPT_AND_EXECUTE` |
| **SUCCEED** | 正常完成 | `goal_handle->succeed(result)` |
| **ABORT** | Server 主動失敗 | `goal_handle->abort(result)` |
| **CANCEL** | Client 要求取消 + Server 接受 | `goal_handle->canceled(result)` |

業界 Nav2 / MoveIt 都會用到全部五種。

---

## 🎬 故事設計：Countdown action

從 N 倒數到 0 的長任務。**規則設計刻意覆蓋所有情境**：

| start_number | 行為 |
|-------------|------|
| `> 30` | server 直接 REJECT（太久不接） |
| `< 0` | server REJECT |
| `13` | 跑到 7 時 server 主動 ABORT（模擬內部錯誤） |
| 其他 1-30 | 正常 SUCCEED |
| 任何值 + Ctrl+C | client 送 cancel → server CANCEL |

---

## 💻 Server 程式碼亮點

完整見 [`code/my_cpp_pkg/src/countdown_server.cpp`](code/my_cpp_pkg/src/countdown_server.cpp)。

### handle_goal 內 reject

```cpp
rclcpp_action::GoalResponse handle_goal(
    const rclcpp_action::GoalUUID & uuid,
    std::shared_ptr<const Countdown::Goal> goal)
{
    if (goal->start_number > 30) {
        RCLCPP_WARN(get_logger(), "Rejecting: %d too large", goal->start_number);
        return rclcpp_action::GoalResponse::REJECT;
    }
    return rclcpp_action::GoalResponse::ACCEPT_AND_EXECUTE;
}
```

**重點**：reject 在 server 接到 goal 的「**第一時間**」就拒絕。client 收到 `goal_response_callback` 帶的 `nullptr` 表示被拒。

### execute thread 內處理三件事

```cpp
void execute(const std::shared_ptr<GoalHandle> goal_handle) {
    rclcpp::Rate rate(1);  // 1Hz

    for (int i = goal->start_number; i >= 0 && rclcpp::ok(); --i) {
        // 1. 檢查 cancel
        if (goal_handle->is_canceling()) {
            result->success = false;
            result->reached = i;
            goal_handle->canceled(result);     // ← cancel
            return;
        }

        // 2. 檢查 abort 條件
        if (goal->start_number == 13 && i == 7) {
            result->success = false;
            result->reached = i;
            goal_handle->abort(result);        // ← abort
            return;
        }

        // 3. 發送 feedback
        feedback->current_number = i;
        goal_handle->publish_feedback(feedback);
        rate.sleep();
    }

    // 4. 正常完成
    result->success = true;
    result->reached = 0;
    goal_handle->succeed(result);              // ← succeed
}
```

**三個結束 API**：
- `succeed(result)` — 正常完成
- `abort(result)` — 主動失敗
- `canceled(result)` — 接受 cancel 並結束

每次 loop 開頭一定要 **檢查 `is_canceling()`** —— 不然 client 送 cancel 也沒人理。

---

## 💻 Client 程式碼亮點

完整見 [`code/my_cpp_pkg/src/countdown_client.cpp`](code/my_cpp_pkg/src/countdown_client.cpp)。

### 三個 callback 各自處理結果

```cpp
opts.goal_response_callback = [this](GoalHandle::SharedPtr handle) {
    if (!handle) {
        RCLCPP_ERROR(get_logger(), "❌ Goal REJECTED");
    } else {
        RCLCPP_INFO(get_logger(), "✅ Goal accepted");
        goal_handle_ = handle;  // 保存給之後 cancel 用
    }
};

opts.feedback_callback = [this](GoalHandle::SharedPtr,
                                 const std::shared_ptr<const Countdown::Feedback> fb) {
    RCLCPP_INFO(get_logger(), "[Feedback] current=%d", fb->current_number);
};

opts.result_callback = [this](const GoalHandle::WrappedResult & wr) {
    switch (wr.code) {
        case rclcpp_action::ResultCode::SUCCEEDED:
            RCLCPP_INFO(get_logger(), "🎉 SUCCEEDED: reached %d", wr.result->reached);
            break;
        case rclcpp_action::ResultCode::ABORTED:
            RCLCPP_ERROR(get_logger(), "💥 ABORTED at %d", wr.result->reached);
            break;
        case rclcpp_action::ResultCode::CANCELED:
            RCLCPP_WARN(get_logger(), "⚠️ CANCELED at %d", wr.result->reached);
            break;
    }
};
```

### SIGINT 觸發 cancel

```cpp
std::shared_ptr<CountdownClient> g_client;

void sigint_handler(int) {
    if (g_client) g_client->cancel_goal();
}

int main(...) {
    rclcpp::init(argc, argv);
    g_client = std::make_shared<CountdownClient>(start);
    g_client->send_goal();

    std::signal(SIGINT, sigint_handler);  // 攔截 Ctrl+C
    rclcpp::spin(g_client);
}

void cancel_goal() {
    if (goal_handle_) {
        client_->async_cancel_goal(goal_handle_);
    }
}
```

按 Ctrl+C → 呼叫 `async_cancel_goal()` → server 收到 cancel request → server 在下個 loop 看到 `is_canceling()` = true → 呼叫 `canceled(result)` → client 收到 `result_callback` with code=CANCELED。

---

## 🚀 Demo 流程

### Step 1：部署 + build

#### 必須先確認 my_robot_interfaces 含 Countdown.action

如果你之前 build 的 my_robot_interfaces 是 Phase 08 第一版（沒 Countdown），先重 build：
```bash
cp /mnt/d/ros_learn/ros2-learning-notes/phase-08-custom-interfaces/code/my_robot_interfaces/action/Countdown.action \
   ~/ros2_ws/src/my_robot_interfaces/action/
cp /mnt/d/ros_learn/ros2-learning-notes/phase-08-custom-interfaces/code/my_robot_interfaces/CMakeLists.txt \
   ~/ros2_ws/src/my_robot_interfaces/
colcon build --packages-select my_robot_interfaces
source install/setup.bash
ros2 interface show my_robot_interfaces/action/Countdown    # 確認看得到
```

#### ☁️ TheConstructSim
```bash
cd ~/ros2_ws/src
ln -s ros2-learning-notes/phase-13-actions-advanced/code/my_cpp_pkg phase13_pkg
```

#### 💻 本機 WSL2
```bash
cp -r /mnt/d/ros_learn/ros2-learning-notes/phase-13-actions-advanced/code/my_cpp_pkg \
      ~/ros2_ws/src/phase13_pkg
sed -i 's|<name>my_cpp_pkg</name>|<name>phase13_pkg</name>|' ~/ros2_ws/src/phase13_pkg/package.xml
sed -i 's|project(my_cpp_pkg)|project(phase13_pkg)|' ~/ros2_ws/src/phase13_pkg/CMakeLists.txt
```

```bash
colcon build --packages-select phase13_pkg
source install/setup.bash
```

### Step 2：啟動 server

Terminal 1：
```bash
ros2 run phase13_pkg countdown_server
# Countdown server ready (try numbers: 1-30)
```

### Step 3：四個情境測試

#### 情境 1: SUCCEED
```bash
ros2 run phase13_pkg countdown_client 5
```
**實測 log（驗證過）**：
```
✅ Goal accepted
[Feedback] current=5
[Feedback] current=4
[Feedback] current=3
[Feedback] current=2
[Feedback] current=1
[Feedback] current=0
🎉 SUCCEEDED: reached 0
```

#### 情境 2: ABORT
```bash
ros2 run phase13_pkg countdown_client 13
```
**實測 log（驗證過）**：
```
✅ Goal accepted
[Feedback] current=13
[Feedback] current=12
... 6 個 feedback ...
[Feedback] current=8
💥 ABORTED at 7
```

#### 情境 3: REJECT
```bash
ros2 run phase13_pkg countdown_client 100
```
**實測 log（驗證過）**：
```
❌ Goal REJECTED by server
```

#### 情境 4: CANCEL
```bash
ros2 run phase13_pkg countdown_client 20
# 等 5 秒看到 [Feedback] current=15 後按 Ctrl+C
```
預期 log：
```
✅ Goal accepted
[Feedback] current=20
[Feedback] current=19
...
[Feedback] current=15
^C
[WARN] Sending cancel request
⚠️ CANCELED at 14
```

### Step 4：用 ros2 action CLI 也能玩

```bash
# 看伺服器有哪些 action
ros2 action list

# 看某 action 的型別
ros2 action info /countdown -t

# 不寫 client 也能送 goal
ros2 action send_goal /countdown my_robot_interfaces/action/Countdown \
  '{start_number: 5}' --feedback
```

---

## 🐛 常見雷

### 雷 1：忘記檢查 `is_canceling()`
```cpp
for (int i = N; i >= 0; --i) {
    // ❌ 沒檢查 → client 送 cancel 也沒人理
    feedback->current_number = i;
    goal_handle->publish_feedback(feedback);
    rate.sleep();
}
```
每次 loop 開頭一定要 `if (goal_handle->is_canceling()) { ...canceled(); return; }`。

### 雷 2：execute 在 callback 內阻塞
```cpp
// ❌ 在 handle_accepted 內直接 sleep → 卡住整個 spin
void handle_accepted(...) { sleep(10); }

// ✅ 開新 thread
void handle_accepted(...) {
    std::thread{...}.detach();
}
```

### 雷 3：result 已 set 卻沒呼叫 succeed/abort/cancel
```cpp
// ❌ 設了 result 但沒結束 goal_handle
result->success = true;
// 忘了呼叫 goal_handle->succeed(result)
return;
```
client 永遠收不到 result。

### 雷 4：handle_cancel 回 REJECT
```cpp
// 業務上你可能不想接受 cancel（例如已經太晚停不下來）
return rclcpp_action::CancelResponse::REJECT;
```
這時 client 雖然送了 cancel，但 server 不會真的 cancel——goal 繼續跑到完成。**正確設計**：根據業務狀態決定接不接受。

### 雷 5：rclcpp_action client shutdown 時 segfault
```
cannot publish data, at ./src/rmw_publish.cpp:62
destroy_client() failed to delete datareader
[ros2run]: Segmentation fault
```
這是 [rclcpp_action 已知 issue](https://github.com/ros2/rclcpp/issues/1888)，Humble 還沒修。**不影響 demo 結果**——result 已經正確收到了，segfault 在 process exit 時才發生。產品環境記得自己管理 client lifetime（例如 `client_.reset()` 在 spin 結束前）。

### 雷 6：rclcpp_action 沒 link
```cmake
find_package(rclcpp_action REQUIRED)
ament_target_dependencies(your_node rclcpp rclcpp_action ...)
```
忘了報「找不到 rclcpp_action::create_server」。

---

## 🎯 學到的關鍵概念

- **5 種 action 結束方式**：REJECT / SUCCEED / ABORT / CANCEL（+ ACCEPT_AND_EXECUTE 是「開始」不是結束）
- **handle_goal**：在 server 第一時間決定接不接
- **handle_cancel**：client 想取消時 server 可選擇接受/拒絕
- **execute thread**：必須開新 thread，不能在 callback 內阻塞
- **`is_canceling()`**：execute 內每次 loop 必檢查
- **三個結束 API**：`succeed()` / `abort()` / `canceled()`
- **client 三個 callback**：goal_response / feedback / result

---

## 🌟 進階挑戰

1. **多 client 排隊**：改 server 用 mutex + queue 處理同時來的多個 goal
2. **goal preemption**：第二個 goal 來時自動 cancel 第一個
3. **timeout**：server 端設 30 秒上限，超時自動 abort
4. **持久化進度**：server 把當前進度寫到檔案，crash 重啟後從中斷點繼續

---

## 下一步

- [Phase 12 — 測試（gtest + launch_testing）](../phase-12-testing/)（待完成）
- [Capstone 1 整合](../phase-14-capstone-1/)（待完成）

---

## 📁 完整檔案結構

```
phase-13-actions-advanced/
├── README.md
└── code/
    └── my_cpp_pkg/
        ├── package.xml
        ├── CMakeLists.txt
        └── src/
            ├── countdown_server.cpp     ← 含 reject/abort/cancel 全套
            └── countdown_client.cpp     ← SIGINT 觸發 cancel
```

> 注意：Countdown.action 定義在 [Phase 08 my_robot_interfaces](../phase-08-custom-interfaces/code/my_robot_interfaces/action/Countdown.action)
