# Phase 13：Actions 進階 🚀

**學完你會**：🌟 寫 Action Server 處理 cancel / abort / reject 三種「非正常結束」、寫 Action Client 中途取消、理解業界 Nav2/MoveIt 的 action 介面為什麼這樣設計。

**前置準備**：
- [Phase 08 Custom Interfaces](../phase-08-custom-interfaces/) — 包含本章用的 Countdown.action interface

**產出目標**：
- [`src/countdown_server.cpp`](code/my_cpp_pkg/src/countdown_server.cpp) — 完整 cancel/abort/reject 邏輯
- [`src/countdown_client.cpp`](code/my_cpp_pkg/src/countdown_client.cpp) — 可用 SIGINT 取消的 client

**環境**：☁️ TheConstructSim + 💻 本機 WSL 雙環境通用。

---

## 🤔 為什麼進階

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

## 🕵️ 終端機偵探課:看業界 Action server 長什麼樣

寫自己的進階 action 之前,先**用 CLI 觀察業界 Nav2 / MoveIt 的 action server**,看他們提供哪些介面、怎麼互動。

### 啟動一個有 action server 的系統

```bash
# Phase 22A 的 Nav2 demo(雲端推薦,GPU 才跑得起來)
ros2 launch my_nav2_demo nav2_demo.launch.py

# 或最簡單:跑 Phase 08 的 smart_brake_v2(它也是 action server)
ros2 run phase08_pkg smart_brake_v2
```

### 看系統內所有 action

```bash
ros2 action list
# /approach              ← Phase 08 寫的
# /navigate_to_pose      ← Nav2 主任務
# /backup                ← Nav2 後退行為
# /spin                  ← Nav2 原地轉
# /follow_path           ← Nav2 沿路徑走
```

### 看單一 action 的型別 + interface

```bash
# 看 type
ros2 action info /navigate_to_pose -t
# Action: /navigate_to_pose
#   Action clients: 1
#   Action servers: 1 [/bt_navigator]
# Type: nav2_msgs/action/NavigateToPose

# 看 interface 結構(Goal / Result / Feedback 三段)
ros2 interface show nav2_msgs/action/NavigateToPose
```

### 從 CLI 直接送 goal 試試看

```bash
# 給 Nav2 送導航目標
ros2 action send_goal /navigate_to_pose nav2_msgs/action/NavigateToPose \
  "{pose: {header: {frame_id: map}, pose: {position: {x: 1.0, y: 1.0}}}}" \
  --feedback
# --feedback 會 print 中途 feedback,看到「distance_remaining 從 5.0 → 0.0」
# Ctrl+C 會送 cancel 過去
```

**💡 劃重點：觀察業界大神的 Action 介面設計哲學**
- **Goal 通常是包含大量細節的複雜結構體**：業界不會只傳遞一個單純的數字。例如 Nav2 的目標通常會包裝成 `geometry_msgs/PoseStamped`，裡面精確包含了目標座標、姿態以及 Header (時間戳記和座標系)。這才是設計企業級通訊介面的正確心法。
- **Feedback 應該具備高度的「 UI 友善性」**：Feedback 的設計目的，就是要讓 Client 能夠輕鬆地在網頁端或 Rviz 上畫出漂亮的「進度條」。所以通常會設計成包含「當前進度百分比」、「剩餘距離」與「預估剩餘時間」，而不只是單調的一堆座標變化。
- **Result 越簡單越好**：因為在任務執行的漫長過程中，Client 已經透過 Feedback 充分掌握了所有的狀態細節。所以最後的 Result 通常只需要回傳簡單的 `bool success` 或者一個總結性的整數代碼，不需要再把整個軌跡資料重新傳送一次。
- **Cancel 絕對是長任務的標配**：在真實世界中，任何耗時超過 3 秒的任務（例如機器人走去廚房），都必須無條件支援「中途取消 (Cancel)」。這是防呆與安全機制的核心，如果不寫 Cancel 邏輯，你的 Action 就只是個半成品。

**我們這章的目標**:寫一個 server 涵蓋 5 種結束方式(REJECT / ACCEPT / SUCCEED / ABORT / CANCEL),寫 client 演示中途 Ctrl+C 觸發 cancel。學完這章你看 Nav2 / MoveIt action server 內部就懂了。

---

## 🎬 故事設計:Countdown action

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

**💡 劃重點**：reject 在 server 接到 goal 的「**第一時間**」就拒絕。client 收到 `goal_response_callback` 帶的 `nullptr` 表示被拒。

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

- **5 種 Action 結束與狀態切換**：徹底理解 REJECT、SUCCEED、ABORT 與 CANCEL 四種結束分支，以及代表任務正式展開的 ACCEPT_AND_EXECUTE。
- **防禦性設計的第一關 (`handle_goal`)**：這是 Server 收到請求時的第一個檢查點，你必須在這裡立刻過濾掉所有不合理或非法的 Goal（例如給出負數的距離），並用 REJECT 將其阻擋在外。
- **取消請求的決策權 (`handle_cancel`)**：Client 有權利隨時喊停，但 Server 擁有最終的決定權。如果任務已經進行到「不可逆」的階段（例如機械臂已經夾緊玻璃杯並舉在半空），Server 絕對有權利拒絕這個取消請求以保護硬體。
- **絕不能阻塞的主幹道 (Execute Thread)**：Action 最容易踩的坑，就是在 `execute` 內直接寫死迴圈。記住，必須開一條新的 Thread (執行緒) 來跑長任務，否則整個 Node 的事件迴圈會卡死，連 Feedback 都發不出去。
- **持續監聽取消指令 (`is_canceling()`)**：在 Execute 的耗時迴圈中，每一次迭代 (Loop) 都必須乖乖檢查 `is_canceling()` 標誌。如果不檢查，Client 就算喊破喉嚨，Server 也會無視取消指令繼續跑完。
- **精準呼叫結束 API**：在任務執行完畢的那個瞬間，務必明確呼叫 `succeed()`、`abort()` 或 `canceled()`，並且附上打包好的 Result，否則 Client 會永遠在痴痴地等。
- **Client 端的接球三兄弟**：身為呼叫方，你必須實作三個 Callback 來完美接球：`goal_response` (確認有沒有被拒絕)、`feedback` (更新 UI 進度條)、以及 `result` (任務結算畫面)。

---

## 🌟 進階挑戰

1. **多 client 排隊**：改 server 用 mutex + queue 處理同時來的多個 goal
2. **goal preemption**：第二個 goal 來時自動 cancel 第一個
3. **timeout**：server 端設 30 秒上限，超時自動 abort
4. **持久化進度**：server 把當前進度寫到檔案，crash 重啟後從中斷點繼續

---

## 👣 下一步去哪？

- [Phase 12 — 測試（gtest + launch_testing）](../phase-12-testing/)
- [Capstone 1 整合](../phase-14-capstone-1/)

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
