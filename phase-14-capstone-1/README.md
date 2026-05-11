# 🎯 Capstone 1：整合作品 — ApproachController

> Part 3 結業專案。把 Phase 08–13 學的所有東西**整合**成一個能 demo 給人看、能放 GitHub 當 portfolio 的作品。

**這個 capstone 證明你會做什麼**：

- **✅ 從零設計領域專屬的 ROS 訊息協議 (Phase 08)**：你不再只是拿別人的標準型別來套用，而是親手制定了系統專屬的 `.msg`、`.srv` 與 `.action` 介面，這是系統架構師的基本功。
- **✅ 實作受控的 LifecycleNode 狀態機 (Phase 09)**：放棄了陽春的普通 Node，改用工業級別的 Lifecycle 節點，精準掌控了資源初始化 (`configure`) 與邏輯啟動 (`activate`) 的生命週期。
- **✅ 用 Launch Event Handler 實現自動化部署 (Phase 11)**：利用事件驅動的特性，讓你的啟動腳本充滿智慧——「等節點的 Process 真正活起來後，再自動幫它切換 Activate 狀態」，完全不需要人工手動介入。
- **✅ 抽離純邏輯並撰寫嚴謹的單元測試 (Phase 12)**：展現了優良的軟體工程素養，把數學計算邏輯與 ROS 底層通訊完美解耦，並用 Google Test 覆蓋了所有的極端邊角情境。
- **✅ 實戰最高難度的 Action Server (Phase 13)**：不只會寫「順利跑完」的理想任務，還能優雅地處理「中途被使用者強制取消 (Cancel)」或「發生系統意外而果斷放棄 (Abort)」的複雜非同步流程。
- **✅ 六神合一的全能節點**：在同一個 Node 裡面，安全且流暢地管理 6 種不同的 ROS 通訊角色，並且完美的處理了執行緒間的同步問題。

**對應業界職位的能力**：「能獨立設計+實作一個 ROS 子系統」。

**前置準備**：
- [Phase 08](../phase-08-custom-interfaces/) ~ [Phase 13](../phase-13-actions-advanced/) 全部
- 特別是 `my_robot_interfaces` 必須先 build

**產出目標**：
- [`src/approach_controller.hpp`](code/my_cpp_pkg/src/approach_controller.hpp) — 純邏輯 SpeedPolicy
- [`src/approach_controller.cpp`](code/my_cpp_pkg/src/approach_controller.cpp) — 完整 LifecycleNode（260+ 行）
- [`launch/capstone.launch.py`](code/my_cpp_pkg/launch/capstone.launch.py) — event_handler 自動化
- [`test/test_speed_policy.cpp`](code/my_cpp_pkg/test/test_speed_policy.cpp) — 5 個單元測試

**環境**：☁️ TheConstructSim + 💻 本機 WSL 雙環境通用。

---

## 🏗️ 系統架構

```
                                  ┌──────────────────────────┐
                                  │  approach_controller     │
                                  │  (LifecycleNode)         │
                                  │                          │
fake_lidar ───/lidar_points ───▶ │ Sub: PointCloud2         │
                                  │                          │
                                  │ Action Server: approach  │ ◀── action client
                                  │                          │
                                  │ Service: set_brake_mode  │ ◀── ros2 service call
                                  │                          │
                                  │ Pub: cmd_vel ─────────▶ turtlesim / 真實機器人
                                  │ Pub: brake_status ────▶ rqt_plot / 監控系統
                                  └──────────────────────────┘
                                          ▲
                              Lifecycle: ros2 lifecycle set
                              Launch: 自動 configure → activate
```

**6 種 ROS 角色**全在這一個 Node 裡：
1. **Sub** PointCloud2
2. **Pub** Twist (cmd_vel)
3. **Pub** BrakeStatus (custom)
4. **Service Server** SetBrakeMode (custom)
5. **Action Server** Approach (custom)
6. **LifecycleNode** 全套狀態機

---

## 💻 重點程式碼

### 1. 純邏輯抽出來（Phase 12 教的）

```cpp
// approach_controller.hpp - 不依賴 rclcpp，可獨立單元測試
class SpeedPolicy {
public:
    double compute_speed(double obstacle_distance, double max_speed) const {
        if (obstacle_distance < 0.0) return 0.0;
        if (obstacle_distance > safe_distance_) return max_speed;
        return max_speed * slowdown_factor_;
    }
    bool reached_target(double obstacle_distance, double target) const { ... }
};
```

### 2. LifecycleNode 完整生命週期（Phase 09 教的）

```cpp
class ApproachController : public rclcpp_lifecycle::LifecycleNode {
    LifecycleCallbackReturn on_configure(...) override {
        cmd_pub_ = create_publisher<...>(...);   // 建立資源
        status_pub_ = create_publisher<...>(...);
        return SUCCESS;
    }

    LifecycleCallbackReturn on_activate(...) override {
        cmd_pub_->on_activate();                  // 啟用
        // 建立 Subscriber + Service + Action + Timers
        return SUCCESS;
    }

    LifecycleCallbackReturn on_deactivate(...) override {
        // 停 timers + 關閉 sub/srv/action
        return SUCCESS;
    }
};
```

### 3. Action Server 完整 cancel/abort/succeed（Phase 13 教的）

```cpp
void execute_approach(const std::shared_ptr<ApproachGoalHandle> goal_handle) {
    // 暫存原模式，任務結束復原
    uint8_t saved_mode = mode_.load();
    double saved_max = max_speed_.load();
    mode_ = BrakeStatus::MODE_ENABLED;
    max_speed_ = goal->approach_speed;

    rclcpp::Rate rate(5);
    while (rclcpp::ok()) {
        if (goal_handle->is_canceling()) { /* canceled */ return; }

        // 用 SpeedPolicy 純邏輯類別檢查（從 Phase 12 抽出來那個）
        if (policy_.reached_target(dist, goal->target_distance)) {
            goal_handle->succeed(result);
            mode_ = saved_mode; max_speed_ = saved_max;
            return;
        }

        if (elapsed > 30.0) { /* abort */ return; }

        rate.sleep();
    }
}
```

### 4. Launch 自動化生命週期（Phase 11 教的）

```python
# capstone.launch.py
controller = LifecycleNode(...)

return LaunchDescription([
    controller,

    # process 起來 → 自動送 configure
    RegisterEventHandler(
        OnProcessStart(
            target_action=controller,
            on_start=[
                LogInfo(msg='✅ controller up, sending configure'),
                EmitEvent(event=ChangeState(transition_id=TRANSITION_CONFIGURE)),
            ]
        )
    ),

    # 1.5 秒後自動送 activate
    TimerAction(
        period=1.5,
        actions=[
            LogInfo(msg='✅ sending activate'),
            EmitEvent(event=ChangeState(transition_id=TRANSITION_ACTIVATE)),
        ]
    ),
])
```

### 5. 單元測試（Phase 12 教的）

```cpp
// test_speed_policy.cpp
TEST(SpeedPolicyTest, FullSpeedWhenClear) {
    SpeedPolicy policy(1.0, 0.3);
    EXPECT_DOUBLE_EQ(policy.compute_speed(5.0, 0.5), 0.5);
}

TEST(SpeedPolicyTest, SlowsDownNearObstacle) {
    SpeedPolicy policy(1.0, 0.3);
    EXPECT_NEAR(policy.compute_speed(0.5, 0.5), 0.15, 1e-9);
}
// ...5 個測試全 pass
```

---

## 🚀 完整 Demo 流程

### Step 1：確認 my_robot_interfaces 已 build（Phase 08 + Phase 13 擴充版）

需要含 `BrakeStatus.msg`、`SetBrakeMode.srv`、`Approach.action`、`Countdown.action`：

```bash
ros2 interface list | grep my_robot
```

### Step 2：部署 + build

#### ☁️ TheConstructSim
```bash
cd ~/ros2_ws/src
ln -s ros2-learning-notes/phase-14-capstone-1/code/my_cpp_pkg phase14_pkg
```

#### 💻 本機 WSL2
```bash
cp -r /mnt/d/ros_learn/ros2-learning-notes/phase-14-capstone-1/code/my_cpp_pkg \
      ~/ros2_ws/src/phase14_pkg
sed -i 's|<name>my_cpp_pkg</name>|<name>phase14_pkg</name>|' ~/ros2_ws/src/phase14_pkg/package.xml
sed -i 's|project(my_cpp_pkg)|project(phase14_pkg)|' ~/ros2_ws/src/phase14_pkg/CMakeLists.txt
```

```bash
cd ~/ros2_ws
colcon build --packages-select phase14_pkg
source install/setup.bash
```

### Step 3：跑單元測試

```bash
colcon test --packages-select phase14_pkg
colcon test-result --test-result-base build/phase14_pkg/test_results
```

預期：`Summary: 5 tests, 0 errors, 0 failures, 0 skipped`

### Step 4：用 launch 啟動（自動 lifecycle）

```bash
ros2 launch phase14_pkg capstone.launch.py
```

**實測 log（驗證過）**：
```
[approach_controller-1]: process started with pid [1506]
[launch.user]: ✅ controller process up, sending configure
[approach_controller]: [Capstone] Constructor done (state: unconfigured)
[approach_controller]: [Capstone] on_configure
[launch.user]: ✅ sending activate
[approach_controller]: [Capstone] on_activate   ← Node 完全就緒
```

### Step 5：跟 controller 互動

新 terminal 跑 fake_lidar：
```bash
python3 ~/fake_lidar.py 0.5
```

**Demo A**：看 BrakeStatus topic
```bash
ros2 topic echo /brake_status
# 看到 mode/speed/distance/uptime/text 完整訊息
```

**Demo B**：呼叫 Service 進 EMERGENCY 模式
```bash
ros2 service call /set_brake_mode my_robot_interfaces/srv/SetBrakeMode \
  '{mode: 2, max_speed: -1.0, reason: emergency}'
# 立刻看到 cmd_vel.linear.x = 0
```

**Demo C**：送 Approach action
```bash
ros2 action send_goal /approach my_robot_interfaces/action/Approach \
  '{target_distance: 0.5, approach_speed: 0.3}' --feedback
# 看到 feedback 流動 + 最終 result
```

**Demo D**：手動切 lifecycle
```bash
ros2 lifecycle set /approach_controller deactivate
# Node 停止運作（但保留設定）

ros2 lifecycle set /approach_controller activate
# 恢復
```

---

## 🎯 為什麼這個 Capstone 強

學完 Part 1+2 你能寫一個基本機器人控制 Node。學完這個 Capstone 你能：

| 能力 | 對應的職位需求 |
|------|--------------|
| 設計領域 .msg/.srv/.action | 任何 ROS 職位 |
| LifecycleNode 設計 | Nav2 開發、自駕車 |
| Launch 自動化生命週期 | 系統整合工程師 |
| 抽純邏輯做測試 | 任何要求「會寫測試」的職位 |
| Action 全套（cancel/abort/succeed） | Nav2、MoveIt 開發 |

**履歷怎麼寫**：
> 「設計並實作一個整合的避障控制系統。包含自訂 ROS 介面（msg/srv/action）、LifecycleNode 生命週期管理、Launch event_handler 自動化、單元測試覆蓋核心控制邏輯、Action server 處理長時間任務的完整錯誤分支（cancel/abort/timeout）。GitHub: [link]」

---

## 🔬 怎麼進階

完成 Capstone 1 的下一步：

1. **挑戰極致效能 (Composition)**：回顧 Phase 09，試著把 `ApproachController` 改寫成 ComposableNode，並跟模擬光達 (fake_lidar) 的程式打包進同一個 Process 裡，享受 Zero-copy 的極速通訊。
2. **系統級自動化測試 (launch_testing)**：撰寫一個 `.test.py` 整合測試腳本，讓系統能在背景自動跑起來，並自動驗證「啟動後 5 秒內，`approach_controller` 的狀態機是否確實正確切換到了 active」。
3. **建立專業的持續整合 (CI) 流水線**：把這包程式碼推上 GitHub，並撰寫 GitHub Actions 設定檔。讓未來每一次的 Push 或 PR，雲端伺服器都會自動幫你跑 `colcon build` 與 `colcon test`，確保專案永遠保持在可運作的完美狀態。
4. **真槍實彈上機測試 (Real Robot)**：準備一台裝了 ROS 2 Humble 的 TurtleBot3 或任何實體移動平台。把假的光達程式拿掉，接上真正的雷射測距儀，體驗軟體走入現實世界的感動。

每個都是真實業界做的事。

---

## 🐛 常見雷

### 雷 1：launch lifecycle 沒成功 activate
```python
# ❌ lifecycle_node_matcher 條件錯
EmitEvent(event=ChangeState(...))   # 沒指定 matcher 會錯

# ✅ 用 lambda 接受任何 lifecycle action
EmitEvent(event=ChangeState(
    lifecycle_node_matcher=lambda action: True,
    transition_id=...,
))
```

### 雷 2：on_activate 漏 publisher activation
```cpp
// LifecyclePublisher 必須手動 activate（Phase 09 教過）
cmd_pub_->on_activate();
status_pub_->on_activate();
```

### 雷 3：Action 在 cancel/abort 時沒復原狀態
```cpp
// 任務開始時暫存
uint8_t saved_mode = mode_.load();

// 不管 succeed / cancel / abort 都要 restore
mode_ = saved_mode;
```

不寫 → 任務結束後系統留在錯誤狀態。

### 雷 4：純邏輯 class 寫了但 main code 沒用
```cpp
// 本 capstone 的 publish_cmd 內：
twist.linear.x = policy_.compute_speed(dist, max_v);  // ✅ 用 SpeedPolicy
//             ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
//   保證 production 邏輯跟測試的邏輯是同一份
```

寫了但沒用 → 測試 pass 但 production 還是錯的。

---

## 📁 完整檔案結構

```
phase-14-capstone-1/
├── README.md
└── code/
    └── my_cpp_pkg/
        ├── package.xml
        ├── CMakeLists.txt
        ├── src/
        │   ├── approach_controller.hpp     ← 純邏輯（被測試）
        │   └── approach_controller.cpp     ← LifecycleNode 完整實作
        ├── launch/
        │   └── capstone.launch.py          ← event_handler 自動化
        └── test/
            └── test_speed_policy.cpp       ← 5 個單元測試
```

---

## 🏆 你做到了什麼

完成 Capstone 1 = **完成 Part 3「系統設計」全章**。

```
✅ Part 1: 通訊基礎
✅ Part 2: 工具與治理 + Mini Capstone 1
✅ Part 3: 系統設計 + Capstone 1   ← 你在這裡
⬜ Part 4: 機器人形體（URDF/TF2/Gazebo/ros2_control/pluginlib/多機通訊）
⬜ Part 5: 領域應用（SLAM/Nav2 OR MoveIt）
⬜ Part 6: 生產化部署（Docker/CI/DDS/實機）
```

下一步進 [Part 4 — 給 Node 們一個身體](../ROADMAP.md#-part-4機器人形體--給-node-們一個身體)。
