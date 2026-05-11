# Phase 26：DDS QoS 調校 🚀

> Part 6 第一章。把 ROS 2 通訊從「能動」變「動得對」。

**這章你將解鎖的業界 DDS 網路技能**：
- **掌握通訊的五大維度 (QoS Policies)**：從 ROS 1 到 ROS 2 最核心的轉變，就是學會控制封包的「可靠性 (Reliability)」、「耐久性 (Durability)」、「歷史紀錄 (History)」、「傳輸期限 (Deadline)」與「活躍狀態 (Liveliness)」。
- **終結「幽靈節點」的夢魘**：看穿那些「節點活著，Topic 存在，卻連一筆資料都收不到」的靈異現象。學會如何從 Console 的警告訊息中，精準抓出 QoS 不匹配 (Incompatible QoS) 的病因。
- **針對場景量身打造通訊策略**：學會不再盲目使用預設值。針對控制指令 (Cmd_vel)、雷射點雲 (Lidar)、靜態地圖 (Map)，精確地配置符合業界標準的 QoS 參數組合。
- **掌握歷史重現的魔法 (Transient Local)**：深刻理解為什麼 `/robot_description` 與 `/map` 這些只發佈一次的訊息，能讓晚加入的 RViz 瞬間讀取到歷史紀錄。

**前置準備**：[Phase 02 設計哲學](../phase-02-communication-concepts/) — 知道 ROS 2 = DDS 的觀念。

**產出目標**：[`code/my_cpp_pkg/`](code/my_cpp_pkg/) — qos_demo + qos_subscriber 兩個可組合驗證 QoS 行為的程式。

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

**業界的血淚經驗談（Reliability 怎麼選？）**：
- **致命的控制指令 (`cmd_vel`, Action Goal)**：絕對必須使用 `Reliable`。當你下達煞車指令時，寧可封包因為網路延遲而慢了幾毫秒抵達，也絕對不能允許它在空中遺失，導致機器人撞牆。
- **高頻的感測器巨獸 (雷射點雲, 高解析度影像)**：強烈建議使用 `Best Effort`。這類資料每秒鐘發送數十甚至數百次。如果為了重傳一個遺失的影像幀而卡住後續的即時畫面，會導致感知系統計算出完全過時的結果。丟失一兩幀無所謂，拿到「最新」的畫面才是最重要的。
- **系統的黑盒子 (`rosout`, TF)**：通常設定為 `Reliable`。Debug 日誌與座標轉換紀錄是工程師除錯的命脈，如果因為網路擁塞而漏失了關鍵的錯誤訊息，將會對事後咎責帶來毀滅性的打擊。

### 2. Durability：耐久性

```
Volatile（預設）：
  publisher 發 → subscriber 在線才收得到。
  晚加入的 subscriber 拿不到歷史。

Transient Local（latched）：
  publisher 緩存最後 N 筆。
  晚加入的 subscriber 一連上立刻收到歷史。
```

**什麼時候該用 Transient Local (Latched)？**
- **機器人的骨架 (`/robot_description`)**：這個 Topic 通常只在啟動時發佈一次。如果不用 `Transient Local` 將它快取起來，晚一分鐘啟動的 RViz 就會因為收不到模型資料，而在畫面上顯示一堆純白的骨架錯誤。
- **靜態地圖 (`/map`)**：不管是由 Map Server 讀取的靜態地圖，還是 SLAM 偶爾更新一次的全域地圖。導航節點隨時都可能重新啟動，它必須能在重啟的瞬間，立刻拿到上一秒存在的地圖。
- **一次性的配置參數 (Configuration Topic)**：某些不常變動但至關重要的系統狀態（例如：目前的工作模式、充電站的絕對座標）。與其讓 Publisher 每秒無意義地重複發送，不如發一次並讓底層 DDS 幫你把這筆資料存起來，等別人需要時自動補發給他。

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

- **通訊的五個旋鈕 (QoS Policies)**：我們不再只能選擇「保證送到」或「隨便傳傳」。我們可以透過調整可靠性、耐久度、歷史深度等五個維度，在「CPU/頻寬效能」與「資料完整性」之間取得完美的平衡。
- **強者可以將就，弱者不能勉強 (相容性法則)**：這是一條鐵則。一個承諾 `Reliable` (保證送達) 的發布者，可以降級去滿足一個只要求 `Best Effort` (盡力就好) 的訂閱者。但反過來，一個只願意 `Best Effort` 的發布者，永遠無法滿足訂閱者 `Reliable` 的苛刻要求。
- **Latched 的化學公式 (`Transient Local` + `Reliable`)**：這是新手最容易犯的錯。單獨設定耐久度是不夠的，在 DDS 的底層規範中，你必須同時保證這兩者，才能啟動類似 ROS 1 中 `latched=True` 的歷史快取魔法。
- **感測器的最佳拍檔 (`SensorDataQoS`)**：不要再手寫 Best Effort 加上 Keep Last 5 了。ROS 2 官方早就準備好這個快捷設定，所有頻寬怪獸（如光達、相機）都應該無腦套用這組參數。
- **終端機裡的照妖鏡 (`ros2 topic info -v`)**：這是你遇到網路靈異事件時的第一件武器。它會扒開 Topic 的外衣，讓底層所有節點的 QoS 設定原形畢露。
- **收不到訊息的元兇**：再強調一次，在排除了網路實體斷線的問題後，ROS 2 系統中「節點活著卻收不到資料」的災情，有 90% 都是因為你在 Publisher 和 Subscriber 之間寫了不相容的 QoS 所導致的。

---

## 🌟 進階挑戰

1. **量測 Reliable vs Best Effort 的差異**：寫高頻率 publisher，故意網路降頻，比較兩種 QoS 的丟包率
2. **Deadline + Liveliness**：寫 callback 監聽 deadline_callback，故意讓 publisher 慢一拍看會不會觸發
3. **跨機器人**：兩台機器（或兩個 ROS_DOMAIN_ID）測試 transient_local 在跨網路的行為
4. **替換 RMW**：改用 CycloneDDS（`export RMW_IMPLEMENTATION=rmw_cyclonedds_cpp`）跑同一份程式比較行為差異

---

## 👣 下一步去哪？

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
