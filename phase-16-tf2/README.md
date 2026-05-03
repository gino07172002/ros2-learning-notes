# Phase 16：TF2 進階 — 操作座標轉換樹

**學完你會**：用 C++ 寫 TF broadcaster 與 listener、處理 TF 時間戳、看懂 TF 例外、用 CLI 工具 debug TF tree。

**前置**：[Phase 15 URDF](../phase-15-urdf/) — 有 TF tree 才有東西可玩。

**產出**：
- [`src/static_broadcaster.cpp`](code/my_cpp_pkg/src/static_broadcaster.cpp) — 發送固定 TF
- [`src/dynamic_broadcaster.cpp`](code/my_cpp_pkg/src/dynamic_broadcaster.cpp) — 發送會變的 TF（圓周運動）
- [`src/tf_listener.cpp`](code/my_cpp_pkg/src/tf_listener.cpp) — 訂閱 TF 並查詢

**環境**：☁️ TheConstructSim + 💻 本機 WSL 雙環境通用。

---

## 為什麼 TF2 是核心大魔王

**只有 URDF 不夠**。URDF 描述機器人**自己**的形狀，但機器人在世界中**移動**時呢？

```
固定的（URDF 提供）：     動態的（程式發送）：
  base_link → lidar       world → base_link  ← 機器人在世界中的位置
              wheel       map → odom         ← SLAM 校正後的座標系
                          odom → base_link   ← 輪式里程計
```

**TF2 解決的問題**：當你在 lidar 座標系拿到一個點，怎麼知道它在 world 座標系的位置？

```
lidar_point (0.5, 0, 0)              ─lookup_transform()→     world_point (?, ?, ?)
in lidar frame                                                  in world frame
```

TF2 自動串連整條鏈路：lidar → base_link → odom → map → world，把每段 transform 乘起來給你最終答案。

業界場景（每天都在做）：
- 光達點雲（lidar frame）→ 地圖（map frame）做 SLAM
- 攝影機看到障礙物（camera frame）→ 機器人座標系（base_link）→ 用來避障
- 機械臂末端（gripper frame）→ 工件座標系（world）→ 規劃抓取軌跡

---

## 🏗️ TF2 三個角色

| 角色 | 做什麼 | 對應 API |
|------|-------|---------|
| **Static Broadcaster** | 發送不變的 TF（一次性，會 latched） | `tf2_ros::StaticTransformBroadcaster` |
| **Dynamic Broadcaster** | 發送會變的 TF（持續發送） | `tf2_ros::TransformBroadcaster` |
| **Listener** | 訂閱 TF tree，查詢任意 transform | `tf2_ros::Buffer + TransformListener` |

**robot_state_publisher** 內部就是用上面的 broadcaster——把 URDF 結構持續發成 TF。

---

## 📝 三個範例

### 1. Static Broadcaster（[`static_broadcaster.cpp`](code/my_cpp_pkg/src/static_broadcaster.cpp)）

```cpp
broadcaster_ = std::make_shared<tf2_ros::StaticTransformBroadcaster>(this);

geometry_msgs::msg::TransformStamped t;
t.header.stamp = now();
t.header.frame_id = "world";        // 父
t.child_frame_id = "my_sensor";     // 子
t.transform.translation.x = 1.0;
t.transform.translation.y = 2.0;
t.transform.translation.z = 0.5;
t.transform.rotation.w = 1.0;       // ⚠️ 必須設！沒旋轉時是 (0,0,0,1)

broadcaster_->sendTransform(t);
```

**Static TF 用 `/tf_static` topic（latched）**——訂閱者一連上就拿到，不像普通 topic 要等下次發送。

### 2. Dynamic Broadcaster（[`dynamic_broadcaster.cpp`](code/my_cpp_pkg/src/dynamic_broadcaster.cpp)）

```cpp
broadcaster_ = std::make_shared<tf2_ros::TransformBroadcaster>(this);

timer_ = create_wall_timer(20ms, [this]() {  // 50Hz
    geometry_msgs::msg::TransformStamped tf_msg;
    tf_msg.header.stamp = now();              // ⚠️ 每次更新 stamp
    tf_msg.header.frame_id = "world";
    tf_msg.child_frame_id = "base_link";

    // 圓周運動
    double t = (now() - start_).seconds();
    tf_msg.transform.translation.x = std::cos(t);
    tf_msg.transform.translation.y = std::sin(t);

    tf2::Quaternion q;
    q.setRPY(0, 0, t + M_PI/2);                // yaw 沿切線
    tf_msg.transform.rotation = tf2::toMsg(q);

    broadcaster_->sendTransform(tf_msg);
});
```

**頻率慣例**：50Hz–100Hz 是正常的 odom 速率。太低 SLAM 會卡，太高浪費 CPU。

### 3. Listener（[`tf_listener.cpp`](code/my_cpp_pkg/src/tf_listener.cpp)）

```cpp
// ⚠️ Buffer 必須給 clock，這是 ROS 2 跟 ROS 1 的差異
tf_buffer_ = std::make_unique<tf2_ros::Buffer>(get_clock());
tf_listener_ = std::make_shared<tf2_ros::TransformListener>(*tf_buffer_);

// 查 world ← base_link
try {
    auto t = tf_buffer_->lookupTransform(
        "world",                    // target
        "base_link",                // source
        tf2::TimePointZero);        // 「最新可用的」

    RCLCPP_INFO(get_logger(), "x=%.2f y=%.2f", 
                t.transform.translation.x, t.transform.translation.y);
} catch (const tf2::TransformException & e) {
    // ⚠️ 必接！剛啟動 TF 還沒到、時間戳對不上都會 throw
    RCLCPP_WARN(get_logger(), "Could not transform: %s", e.what());
}
```

`tf2::TimePointZero` = 「最新可用的」TF，業界 90% 用這個。需要特定時間（例如「拍照那一刻機器人在哪」）才用具體時間戳。

---

## 🚀 Demo 流程

### Step 1：部署 + build

#### 兩種環境通用
```bash
cp -r /mnt/d/ros_learn/ros2-learning-notes/phase-16-tf2/code/my_cpp_pkg \
      ~/ros2_ws/src/phase16_pkg
sed -i 's|<name>my_cpp_pkg</name>|<name>phase16_pkg</name>|' ~/ros2_ws/src/phase16_pkg/package.xml
sed -i 's|project(my_cpp_pkg)|project(phase16_pkg)|' ~/ros2_ws/src/phase16_pkg/CMakeLists.txt

cd ~/ros2_ws
colcon build --packages-select phase16_pkg
source install/setup.bash
```

### Step 2：Demo 1 — Static TF

Terminal 1：
```bash
ros2 run phase16_pkg static_broadcaster
# Static TF: world → my_sensor at (1.0, 2.0, 0.5)
```

Terminal 2：
```bash
ros2 run tf2_ros tf2_echo world my_sensor
```

**驗證輸出**：
```
At time 0.0
- Translation: [1.000, 2.000, 0.500]      ← 完全等於 code 裡寫的
- Rotation: (0, 0, 0, 1)
```

### Step 3：Demo 2 — Dynamic TF + Listener

Terminal 1（broadcaster 50Hz 發送圓周運動）：
```bash
ros2 run phase16_pkg dynamic_broadcaster
```

Terminal 2（listener 1Hz 查詢）：
```bash
ros2 run phase16_pkg tf_listener
```

**驗證輸出**（每秒不同的 x/y，因為 base_link 在做圓周運動）：
```
[tf_listener_node]: world ← base_link: x=0.05 y=1.00 z=0.00
[tf_listener_node]: world ← base_link: x=-0.81 y=0.58 z=0.00
[tf_listener_node]: world ← base_link: x=-0.93 y=-0.37 z=0.00
[tf_listener_node]: world ← base_link: x=-0.19 y=-0.98 z=0.00
```

🎯 **半徑接近 1**（√(x² + y²) ≈ 1），跟 code 設定的圓周完全吻合。

### Step 4：Demo 3 — 用 CLI 工具看整棵樹

```bash
# 列出所有 frame
ros2 run tf2_tools view_frames
# 產出 frames.pdf

# 看頻率
ros2 topic hz /tf

# 看單一 transform
ros2 run tf2_ros tf2_echo world base_link

# 看靜態 TF
ros2 topic echo /tf_static
```

---

## 🐛 常見雷

### 雷 1：quaternion 沒設 `w=1`
```cpp
t.transform.rotation.w = 1.0;   // ✅ 沒旋轉時必填
// 不寫 → quaternion = (0,0,0,0) → TF 會抱怨「invalid quaternion」
```

### 雷 2：Buffer 沒給 clock
```cpp
// ❌ 預設 constructor 在 ROS 2 已經移除
auto buffer = tf2_ros::Buffer();

// ✅ 必須傳 clock
auto buffer = tf2_ros::Buffer(get_clock());
```

### 雷 3：lookup_transform 沒包 try-catch
```cpp
// ❌ 啟動初期 TF tree 還沒建好 → throw → process crash
auto t = tf_buffer_->lookupTransform(...);

// ✅ 必接 exception
try {
    auto t = tf_buffer_->lookupTransform(...);
} catch (const tf2::TransformException & e) {
    RCLCPP_WARN(get_logger(), "%s", e.what());
}
```

### 雷 4：時間戳順序錯
```cpp
// 假設你在 t=10.0 收到一個感測器訊息
// 想查 t=10.0 那一刻的 TF
auto t = tf_buffer_->lookupTransform(
    "world", "base_link",
    tf2_ros::fromMsg(sensor_msg.header.stamp));   // 用感測器的時間戳

// ❌ 如果 TF 還沒到 t=10 → throw "lookup would require extrapolation into the future"
// ✅ 解法：用 tf2::TimePointZero（最新可用），或加 timeout 等待
auto t = tf_buffer_->lookupTransform(
    "world", "base_link",
    tf2_ros::fromMsg(sensor_msg.header.stamp),
    tf2::durationFromSec(0.1));   // 等最多 0.1 秒
```

### 雷 5：parent / child 順序記反
```cpp
// transform: world → base_link 表示「base_link 在 world 座標系的位置」
t.header.frame_id = "world";        // 父（座標系）
t.child_frame_id = "base_link";     // 子（被定位的物件）

// 業界口訣：lookupTransform(target, source) 查的是「source 在 target 座標系下」
auto t = buffer.lookupTransform("world", "base_link", ...);
// → t 是 base_link 在 world 的位置/姿態
```

### 雷 6：`/tf` vs `/tf_static`
- `/tf`: 普通 topic，每次 publish 訂閱者才收到
- `/tf_static`: latched，**一次發送，新訂閱者立刻拿到歷史**

寫 static_broadcaster 一定要用 `StaticTransformBroadcaster`，不能用普通的（普通的 latched 行為靠 QoS Transient Local 模擬，但慣例是不要）。

---

## 🎯 學到的關鍵概念

- **TF2 三角色**：Static Broadcaster / Dynamic Broadcaster / Listener
- **`/tf` vs `/tf_static`**：動態 vs latched
- **Quaternion 必須有效**（最少 `w=1`）
- **Buffer 必須給 clock**（ROS 2 跟 ROS 1 差異）
- **lookupTransform 必接 try-catch**
- **`tf2::TimePointZero`** 是 90% 場景的選擇
- **業界口訣**：`lookupTransform(target, source)` 查 source 在 target 座標系下

---

## 🌟 進階挑戰

1. **轉換 PointStamped**：訂閱一個 lidar 點，用 `tf_buffer_->transform(point_in, point_out, "world")` 一行轉到 world
2. **TF 時間旅行**：用具體 timestamp 查 0.5 秒前的 TF，看是否能正確內插
3. **TF chain debug**：故意把 broadcaster 關掉幾秒看 listener 的 exception
4. **接 Phase 15 URDF**：寫一個 broadcaster 持續更新 world → base_link，看 RViz 裡的車子在地圖上動

---

## 下一步

- Phase 17 — Gazebo 整合（待完成）
- Phase 18 — ros2_control（待完成）

---

## 📁 完整檔案結構

```
phase-16-tf2/
├── README.md
└── code/
    └── my_cpp_pkg/
        ├── package.xml
        ├── CMakeLists.txt
        └── src/
            ├── static_broadcaster.cpp
            ├── dynamic_broadcaster.cpp
            └── tf_listener.cpp
```
