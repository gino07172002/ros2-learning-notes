# Phase 26：DDS QoS 調校

> Part 6 第一章。把 ROS 2 通訊從「能動」變「動得對」。

**學完你會**：理解 ROS 2 五大 QoS policy、辨識 QoS 不匹配的徵兆、選對 publisher/subscriber QoS、看懂 console 警告、知道何時用 transient_local 做「latched」。

**前置**：[Phase 02 設計哲學](../phase-02-communication-concepts/) — 知道 ROS 2 = DDS 的觀念。

**產出**：[`code/my_cpp_pkg/`](code/my_cpp_pkg/) — qos_demo + qos_subscriber 兩個可組合驗證 QoS 行為的程式。

**環境**：☁️ TheConstructSim + 💻 本機 WSL 雙環境通用。

---

## QoS 是什麼

ROS 1 沒有 QoS，全部用 TCP 強制保證送達。ROS 2 改用 DDS，**訊息傳遞變得可調整**：

| Policy | 選項 | 場景 |
|--------|------|------|
| **Reliability** | Reliable / Best Effort | 重要訊息(指令) vs 可丟訊息(感測器) |
| **Durability** | Volatile / Transient Local | 即時 vs 給遲到的訂閱者也能拿到 |
| **History** | Keep Last(N) / Keep All | 預算記憶體 vs 不能漏 |
| **Deadline** | Duration | 期待至少多久收到一筆 |
| **Liveliness** | Automatic / Manual | publisher 多久沒動就算掛 |

**規則**：publisher 與 subscriber 的 QoS **必須兼容**才能通訊。不兼容會「靜默失敗」（一筆都收不到）。

---

## 🏗️ 兩個關鍵概念

### 1. Reliability：可靠性

```
Reliable (TCP-like)：
  publisher ─[1]─[2]─[3]─▶ subscriber
              ↓     ↓     ↓
            ack  ack  ack    ← 每筆都確認
  訊息掉了會重發，保證到達

Best Effort (UDP-like)：
  publisher ─[1]─[2]─[3]─▶ subscriber
  訊息掉了 = 沒了。但延遲低、CPU 省
```

**業界選擇**：
- 控制指令（cmd_vel）：Reliable
- 感測器資料（雷射、影像）：Best Effort
- log（rosout）：Reliable

### 2. Durability：耐久性

```
Volatile（預設）：
  publisher 發 → subscriber 在線才收得到。
  晚加入的 subscriber 拿不到歷史。

Transient Local（latched）：
  publisher 緩存最後 N 筆。
  晚加入的 subscriber 一連上立刻收到歷史。
```

**業界用途**：
- `/robot_description`：機器人 URDF，**必須是 latched**——RViz 晚啟動也要拿得到
- `/map`：地圖，latched
- 一次性發布的「configuration topic」：latched

---

## 📝 兼容性矩陣（記這張）

| Pub Reliability | Sub Reliability | 兼容? |
|----------------|----------------|-------|
| Reliable | Reliable | ✅ |
| Reliable | Best Effort | ✅ Reliable 可降級 |
| Best Effort | Reliable | ❌ Sub 要保證但 Pub 給不了 |
| Best Effort | Best Effort | ✅ |

| Pub Durability | Sub Durability | 兼容? |
|----------------|----------------|-------|
| Transient Local | Transient Local | ✅ |
| Transient Local | Volatile | ✅ |
| Volatile | Transient Local | ❌ Sub 要歷史但 Pub 沒緩存 |
| Volatile | Volatile | ✅ |

**口訣**：「Pub 提供越多 / Sub 要求越少」就越容易兼容。

---

## 💻 程式碼範例

### Publisher（[`qos_demo.cpp`](code/my_cpp_pkg/src/qos_demo.cpp)）

```cpp
rclcpp::QoS qos(10);                       // 預設：Reliable, Volatile, KeepLast(10)

if (mode == "best_effort") {
    qos.best_effort();
} else if (mode == "latched") {
    qos.transient_local().reliable();      // = latched 行為
} else if (mode == "deadline") {
    qos.deadline(std::chrono::milliseconds(1000));   // 1Hz 期望
}

publisher_ = create_publisher<std_msgs::msg::Int32>("counter", qos);
```

### Subscriber（[`qos_subscriber.cpp`](code/my_cpp_pkg/src/qos_subscriber.cpp)）

跟 publisher 一樣的 QoS 設定 API。**訂閱時也要傳 QoS 物件**：

```cpp
sub_ = create_subscription<std_msgs::msg::Int32>(
    "counter",
    qos,                                    // ← 必須對齊 publisher
    callback);
```

---

## 🚀 Demo 流程

### Step 1：build

```bash
cp -r /mnt/d/ros_learn/ros2-learning-notes/phase-26-dds-qos/code/my_cpp_pkg \
      ~/ros2_ws/src/phase26_pkg
sed -i 's|<name>my_cpp_pkg</name>|<name>phase26_pkg</name>|' ~/ros2_ws/src/phase26_pkg/package.xml
sed -i 's|project(my_cpp_pkg)|project(phase26_pkg)|' ~/ros2_ws/src/phase26_pkg/CMakeLists.txt

cd ~/ros2_ws
colcon build --packages-select phase26_pkg
source install/setup.bash
```

### Step 2：Demo 1 — Reliable + Best Effort 兼容（不對稱）

Terminal 1（Reliable publisher）：
```bash
ros2 run phase26_pkg qos_demo reliable
```

Terminal 2（Best Effort subscriber）：
```bash
ros2 run phase26_pkg qos_subscriber best_effort
```

**驗證輸出**：
```
[Subscriber] Received: 0
[Subscriber] Received: 1
[Subscriber] Received: 2
...
```

**收得到** — Reliable pub 可以「降級」滿足 Best Effort sub。

### Step 3：Demo 2 — QoS 不兼容（最重要）

Terminal 1（**Best Effort** publisher）：
```bash
ros2 run phase26_pkg qos_demo best_effort
```

Terminal 2（**Reliable** subscriber）：
```bash
ros2 run phase26_pkg qos_subscriber reliable
```

**驗證輸出（驗證過）**：

publisher 端：
```
[WARN] New subscription discovered on topic '/counter', requesting incompatible QoS.
       No messages will be sent to it.
       Last incompatible policy: RELIABILITY_QOS_POLICY
```

subscriber 端：
```
[WARN] New publisher discovered on topic '/counter', offering incompatible QoS.
       No messages will be sent to it.
       Last incompatible policy: RELIABILITY_QOS_POLICY
```

🎯 **subscriber 完全收不到任何訊息** — Best Effort pub 不能滿足 Reliable sub 的保證需求。

**業界「沒收到訊息」99% 的原因就是這個**。看到 `incompatible QoS` warning 就知道是這個雷。

### Step 4：Demo 3 — Latched (Transient Local)

Terminal 1：
```bash
ros2 run phase26_pkg qos_demo latched
# 等它跑 5 秒（已經發了 0..9）
```

Terminal 2（**晚加入**的 subscriber）：
```bash
ros2 run phase26_pkg qos_subscriber latched
```

**預期輸出**：
```
[Subscriber] Subscriber QoS: latched
[Subscriber] Received: 9        ← 立刻收到歷史最後一筆
[Subscriber] Received: 10       ← 然後繼續收新的
[Subscriber] Received: 11
```

🎯 **這就是 latched 的核心** — 晚加入的訂閱者也能拿到「狀態的最後值」。`/robot_description` 之所以能用就是這個機制。

### Step 5：用 ros2 topic 工具看 QoS

```bash
# 看一個 topic 的詳細 QoS
ros2 topic info /counter -v

# 輸出範例：
# Type: std_msgs/msg/Int32
# Publisher count: 1
#   QoS profile:
#     Reliability: BEST_EFFORT
#     Durability: VOLATILE
#     Lifespan: Infinite
#     Deadline: Infinite
#     Liveliness: AUTOMATIC
```

業界 debug QoS 必用此命令。

---

## 🐛 常見雷

### 雷 1：「為什麼一筆都收不到？」
**90% 是 QoS 不匹配**。先看 publisher / subscriber 雙方有沒有 `incompatible QoS` warning。

### 雷 2：訂閱感測器資料用 Reliable
```cpp
// ❌ Reliable 對 sensor data 不適合（pub 通常 best_effort）
auto sub = create_subscription<...>("scan", 10, callback);

// ✅ 訂閱感測器資料用 SensorDataQoS
auto sub = create_subscription<...>("scan", rclcpp::SensorDataQoS(), callback);
```

`SensorDataQoS` 預設 = Best Effort + KeepLast(5)，業界慣例。

### 雷 3：以為設了 `transient_local()` 就是 latched
```cpp
// ❌ 只設 durability 不夠
qos.transient_local();

// ✅ Transient Local **必須**搭配 Reliable
qos.transient_local().reliable();
```

DDS 規定 transient_local 必須 reliable，不然不算 latched。

### 雷 4：QoS 物件沒傳給 publisher/subscriber
```cpp
// ❌ 預設 QoS（10）沒生效
auto qos = rclcpp::QoS(10).best_effort();   // 設了
auto pub = create_publisher<...>("topic", 10);   // 但這裡傳數字，新建一個 default QoS

// ✅ 把 qos 物件傳進去
auto pub = create_publisher<...>("topic", qos);
```

### 雷 5：跨網路 deadline 太嚴
```cpp
qos.deadline(std::chrono::milliseconds(10));  // 10ms
```

跨網路一定會有抖動，10ms deadline 經常 violation。**起碼 100ms+** 才合理。

### 雷 6：QoS 改了忘記重 build / 重 source
QoS 是程式碼層面的設定，**改完必須重 build**。改完只重啟程式不會生效。

---

## 🎯 學到的關鍵概念

- **5 個 QoS policy**：Reliability / Durability / History / Deadline / Liveliness
- **Reliability 兼容矩陣**：Reliable Pub 可下接 Best Effort Sub，反過來不行
- **Transient Local + Reliable** = latched
- **`SensorDataQoS()`** 是訂閱感測器的標準寫法
- **`ros2 topic info -v`** debug QoS 必用
- **「收不到訊息」90% 是 QoS 不匹配**

---

## 🌟 進階挑戰

1. **量測 Reliable vs Best Effort 的差異**：寫高頻率 publisher，故意網路降頻，比較兩種 QoS 的丟包率
2. **Deadline + Liveliness**：寫 callback 監聽 deadline_callback，故意讓 publisher 慢一拍看會不會觸發
3. **跨機器人**：兩台機器（或兩個 ROS_DOMAIN_ID）測試 transient_local 在跨網路的行為
4. **替換 RMW**：改用 CycloneDDS（`export RMW_IMPLEMENTATION=rmw_cyclonedds_cpp`）跑同一份程式比較行為差異

---

## 下一步

- Phase 27 — 部署實機（待完成）
- Phase 24 — Docker（待完成）

---

## 📁 完整檔案結構

```
phase-26-dds-qos/
├── README.md
└── code/
    └── my_cpp_pkg/
        ├── package.xml
        ├── CMakeLists.txt
        └── src/
            ├── qos_demo.cpp           ← Publisher with switchable QoS
            └── qos_subscriber.cpp     ← Subscriber with switchable QoS
```
