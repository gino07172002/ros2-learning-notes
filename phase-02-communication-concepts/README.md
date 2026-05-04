# Phase 02：ROS 2 設計哲學

**學完你會**：理解 ROS 2 為什麼長這樣——它的通訊模型、與其他框架（MQTT、gRPC、ROS 1）的本質差異、為什麼用 DDS、Node 之間怎麼互相發現。學完之後你看任何 ROS 2 程式碼都知道「為什麼是這樣」。

**前置**：[Phase 01](../phase-01-cloud-env-first-publisher/) — 至少跑過一次 `auto_drive`，知道「Publisher 發出 Twist，turtlesim 收到後動」這個事實。

**產出**：純觀念章節，無 code。但讀完你會脫胎換骨。

---

## 為什麼這章重要

Phase 01 你跑通了一個 demo，但很多事情你還是「不知道為什麼」：
- 為什麼 Publisher 不用知道 Subscriber 在哪？
- 啟動順序為什麼可以隨意？
- topic 的「型別檢查」發生在哪個層？
- 為什麼 ROS 2 跟 ROS 1 完全不一樣？

這章把這些「黑魔法」拆給你看。**不寫 code，但是後續每一章的根**。

---

## 📡 Part 1：通訊模型

### ROS 2 = 「分散式共享發布訂閱匯流排」

最精簡的描述：

```
┌──────────────────────────────────────────────────┐
│              共享匯流排 (DDS)                     │
└──────────────────────────────────────────────────┘
   ↑↓        ↑↓         ↑↓          ↑↓
[Node A] [Node B]  [Node C]   [其他電腦的 Node D]
```

**所有 Node 都連到一條共享匯流排**，講話往匯流排丟、聽話從匯流排撈。沒有中央伺服器。

### 跟你熟悉的東西對照

| 概念 | MQTT | gRPC | ROS 2 |
|------|------|------|-------|
| 發送方稱呼 | publisher | client | publisher / client / action client |
| 接收方稱呼 | subscriber | server | subscriber / server / action server |
| 中央元件 | broker（必要） | 無 | 無（DDS 點對點 discovery）|
| 訊息格式 | byte payload（自訂） | Protobuf（強型別） | IDL → CDR（強型別）|
| 通訊模式 | pub/sub | request/response | pub/sub + request/response + 長任務 |
| 啟動順序 | 要先有 broker | 要先有 server | 任意順序 |

**關鍵差異**：
- **比 MQTT 強**：訊息有強型別（編譯期檢查），不是隨便丟 bytes
- **比 gRPC 強**：原生支援多對多 pub/sub、不需要 client 知道 server 在哪
- **比 ROS 1 強**：沒有 master 單點、可跨機器、支援即時系統

### 三種通訊機制

ROS 2 有三種講話方式，**這是你會反覆用一輩子的東西**：

| 機制 | 比喻 | 場景 | 你已學過 |
|------|------|------|---------|
| **Topic** | 廣播電台 | 連續訊息流（速度指令、感測器數據） | ✅ Phase 01, 03 |
| **Service** | 打電話 | 單次請求 + 回應（開關、查詢） | ✅ Phase 04 |
| **Action** | 叫外送 | 長時間任務 + 進度回報 + 可取消 | Phase 13 會學 |

---

## 🔍 Part 2：DDS — ROS 2 的引擎

ROS 1 用自訂的 TCP 協議 + 中央 master 節點。ROS 2 把整套通訊外包給 **DDS (Data Distribution Service)**——一個工業界既有的標準（航空、軍事、自駕車都在用）。

### 為什麼要外包給 DDS

ROS 2 設計團隊問自己：「為了做機器人通訊，要重新發明所有問題的解法嗎？」答案是不要——DDS 已經解決了這些事 20 年：
- 多對多 publish/subscribe
- 自動 discovery（誰加入誰退出）
- QoS 細粒度控制（reliability、durability、deadline、liveliness…）
- 跨機器、跨網段
- 即時保證

ROS 2 = ROS 1 的觀念 + DDS 的工業級實作。

### 你電腦上的 DDS

ROS 2 可以接多種 DDS 實作（互可替換）：

| DDS 實作 | 預設用在 | 特性 |
|---------|---------|------|
| **FastRTPS / Fast DDS** | Humble 預設 | 開源、輕量 |
| **CycloneDDS** | Iron 之後預設 | 開源、效能佳 |
| **RTI Connext** | 商業客戶 | 付費、最穩定 |

切換用環境變數：
```bash
export RMW_IMPLEMENTATION=rmw_cyclonedds_cpp
```

> 這章不需要切——預設 FastRTPS 用得很順。但你要知道「ROS 通訊不是 ROS 自己做的，是 DDS 做的」。

### DDS 給你帶來的東西

每個 ROS 2 概念在 DDS 裡都對應一個東西：

| ROS 2 | DDS |
|-------|-----|
| Topic | DDS Topic |
| Message type | DDS DataType (IDL 定義) |
| Publisher | DDS DataWriter |
| Subscriber | DDS DataReader |
| QoS profile | DDS QoSPolicy |
| Node | DDS Participant |

**這就是為什麼 ROS 2 通訊有強型別、有 QoS、會自動 discovery**——因為 DDS 本來就有。

---

## 🌐 Part 3：Node Graph 怎麼形成

ROS 2 沒有中央伺服器。那 Node 怎麼互相找到對方？

### Discovery 機制

啟動一個 Node 時：

1. **Node 加入 DDS domain**（用 `ROS_DOMAIN_ID` 區分，預設 0）
2. **多播宣告**：「我來了，我有這些 publisher / subscriber」
3. **既有 Node 回應**：「我有這些 publisher / subscriber，我們有共同的 topic 嗎？」
4. **建立連線**：有匹配的 topic + 兼容的 QoS → 建立直接連線

```
新 Node 加入
   │ 「我有 publisher 'cmd_vel'」
   ▼
[多播 announce]
   ↑↓
其他 Node 收到 → 「我有 subscriber 'cmd_vel'，QoS 對得上！」
   │
   ▼
直接點對點連線（不再經過任何中介）
```

### `ROS_DOMAIN_ID` 的重要性

`ROS_DOMAIN_ID` 是 DDS domain 編號（0–101），**不同 domain 的 Node 完全看不到彼此**。

實務情境：
- 本機開發：`ROS_DOMAIN_ID=0`（預設）
- 跑實機：`export ROS_DOMAIN_ID=42`（避免抓到鄰居的訊息）
- 多人共享網路（公司、學校）：每人各用一個 ID

⚠️ 沒設 `ROS_DOMAIN_ID` 時，**同網段內所有人的 ROS 訊息會混在一起**。學校或共用 WiFi 跑 ROS 是大家的惡夢。

### Discovery 的代價

Discovery 用 UDP 多播，每個新 Node 啟動會送一波廣播。**節點多 (50+) 時會卡**——所以：
- Phase 22 會教 **FastDDS Discovery Server**（中心化 discovery，避免廣播風暴）
- 大規模部署改用「靜態 endpoint」配置

---

## 📦 Part 4：訊息怎麼變成網路封包

這是黑盒中的黑盒。一條完整旅程：

```
你寫的程式：
    auto msg = geometry_msgs::msg::Twist();
    msg.linear.x = 0.2;
    publisher_->publish(msg);
        │
        ▼
[1] rclcpp 把 Twist 物件交給 DDS DataWriter
        │
        ▼
[2] DDS 用 IDL 定義將 Twist 序列化成 CDR (Common Data Representation) 二進位格式
        │
        ▼
[3] CDR bytes 透過 UDP 送出（同機通訊用 shared memory，跨機用網路）
        │
        ▼
[4] 訂閱方 DataReader 收到 CDR bytes
        │
        ▼
[5] 反序列化回 Twist 物件
        │
        ▼
你的 callback：
    void on_msg(const Twist::SharedPtr msg) { ... }
```

**重點**：
- **強型別**從你的 .msg 檔開始（Phase 08 會學自己定義），自動生成 C++/Python class
- 序列化格式 (CDR) 是 DDS 標準，跨語言（C++ publish、Python subscribe 完全 OK）
- 同機通訊**不走網路**——DDS 自動偵測同機就用 shared memory（為什麼 ROS 2 跨節點訊息能 ms 級延遲）

---

## 🆚 Part 5：ROS 2 vs 其他選擇

如果你已經會 MQTT，新公司問你「為什麼不用 MQTT 做機器人？」要會回答。

### vs MQTT

MQTT 缺的：
- ❌ 無強型別（只能傳 bytes，需另外協定）
- ❌ 必須有 broker（單點）
- ❌ 沒有 RPC 機制（service）
- ❌ QoS 只有 0/1/2 三級，不像 DDS 有 20+ 維度

MQTT 強的：
- ✅ 跨網路 NAT 容易（broker 中介）
- ✅ Web/IoT 生態成熟（HTTP/Web port）
- ✅ 簡單

**結論**：物聯網 IoT 用 MQTT 對；機器人用 ROS 2 對。**不要混用**。

### vs gRPC

gRPC 缺的：
- ❌ 沒有 pub/sub（只有 request/response）
- ❌ 沒有跨機 discovery（要自己服務發現）
- ❌ 不是即時的設計

gRPC 強的：
- ✅ Web/微服務生態完整
- ✅ 跨語言更廣（含 Java、Go）
- ✅ 文件好

**結論**：後端微服務用 gRPC；機器人元件之間用 ROS 2。

### vs ROS 1

ROS 1 缺的（也是 ROS 2 為何重寫的原因）：
- ❌ 中央 master（單點故障）
- ❌ 無 QoS 控制
- ❌ 不是即時系統（XML-RPC 慢）
- ❌ 不能跨網段

**結論**：新專案不要碰 ROS 1。維護舊系統才考慮。

---

## 💻 回頭拆解 Phase 01 的 `auto_drive.cpp`

帶著上面的觀念回看 code，會發現一切都很合理。

### 1. 引入工具包

```cpp
#include "rclcpp/rclcpp.hpp"             // ROS 2 C++ 核心 (含 Node 功能)
#include "geometry_msgs/msg/twist.hpp"   // Twist 訊息格式（自動從 IDL 生成的 class）
#include <chrono>
```

### 2. 建立節點 = 加入 DDS domain

```cpp
class AutoDriveNode : public rclcpp::Node
{
public:
    AutoDriveNode() : Node("auto_drive_node")  // 節點命名 = DDS Participant 名稱
    {
```

`Node("auto_drive_node")` 在 DDS 層做了：建立 Participant、加入預設 domain、開始 discovery。

### 3. 註冊 Publisher = 建立 DataWriter

```cpp
        publisher_ = this->create_publisher<geometry_msgs::msg::Twist>("cmd_vel", 10);
```

這行做了：DDS 建立 DataWriter、宣告主題 "cmd_vel"、設定 history depth=10、開始多播 announce。

### 4. Timer callback 在事件迴圈裡跑

```cpp
        timer_ = this->create_wall_timer(
            500ms, std::bind(&AutoDriveNode::timer_callback, this));
```

Timer 不是獨立 thread——它跟所有 callback（subscription、service、parameter 變更）都掛在 `rclcpp::spin()` 的事件迴圈，**單執行緒順序執行**。Phase 09 學的 Executor 就是控制這個迴圈的東西。

### 5. `rclcpp::spin` = 事件迴圈

```cpp
int main(int argc, char * argv[])
{
    rclcpp::init(argc, argv);                        // 初始化 ROS 2 + DDS
    rclcpp::spin(std::make_shared<AutoDriveNode>()); // 阻塞，跑事件迴圈直到 Ctrl+C
    rclcpp::shutdown();                              // 關閉 DDS 連線
    return 0;
}
```

---

## 🏛️ 設計哲學:為什麼是 init / spin / shutdown 三段式

> 這段不是「ROS 2 怎麼用」,是「ROS 2 為什麼這樣設計」。學完之後你不只會用 ROS 2,還能帶走幾條**寫 library 給別人用的設計原則**。
> 詳見 [`DESIGN_NOTES.md`](../DESIGN_NOTES.md) 的長版 + 後續其他主題的設計筆記。

```cpp
rclcpp::init(argc, argv);                        // (1)
rclcpp::spin(std::make_shared<AutoDriveNode>()); // (2)
rclcpp::shutdown();                              // (3)
```

這三行藏著 4 個設計決策。每個都有對應的「**library 設計通則**」可以帶走。

### 決策 A:**Process 內有一個全域 Context,不是每個 Node 各一個**

`rclcpp::init` 不是初始化 Node,是初始化 **整個 process 的 ROS 2 Context**:
- 接管 SIGINT(Ctrl+C 變優雅關閉,不是 kill -9)
- 啟動 DDS Participant Discovery(背景 thread 跑著)
- 解析 `--ros-args`(remap、param 都從這裡來)
- **每個 process 只能呼叫一次**,再呼叫會炸

對應的問題:**「Process 跟 ROS 2 的關係是什麼?」**
答:同一 process 內所有 Node 共享 Context、共享 SIGINT、共享 logger、共享 clock。

#### 跟你熟的東西對比

| 框架 | 等價的「全域初始化」 |
|------|-------------------|
| Python `logging` | `logging.basicConfig()` 一次設好整個 process |
| OpenGL | `glfwInit()` / `glutInit(&argc, argv)` |
| Qt | `QApplication app(argc, argv)` |
| MPI | `MPI_Init(&argc, &argv)` |

ROS 2 跟 MPI、Qt 一樣是「**framework-style**」,**不是 lib-style**。Framework 拿走 main 的控制權(SIGINT、argv),lib 不會。

> **🎓 設計通則 1**:**有全域狀態(thread、signal handler、command-line parsing)時,用顯式的 `init()` 不要藏在 static 初始化**。Static init 順序在跨 translation unit 時是 undefined,且使用者沒辦法插手。`rclcpp::init` 顯式一次 = 可控、可測、可在單元測試裡 mock。

### 決策 B:**Node 是被動資料結構,Executor 才是執行單位**

跟 gRPC / Tornado / Express 都不一樣。看下面對比就懂:

```cpp
// gRPC C++:Server 自己跑
grpc::ServerBuilder builder;
builder.AddListeningPort(...);
builder.RegisterService(&service);
auto server = builder.BuildAndStart();
server->Wait();                     // ← Server 物件自己阻塞
```

```cpp
// ROS 2:Node 不會自己跑
auto node = std::make_shared<AutoDriveNode>();
rclcpp::spin(node);                 // ← 外部 Executor 才是「跑」的動作
```

這個差異看似小,但帶來巨大的彈性:

| 你想做什麼 | gRPC 寫法 | ROS 2 寫法 |
|-----------|----------|-----------|
| 同 process 跑 N 個服務 | N 個 Server,N 個 thread | 1 個 Executor,N 個 Node 共用 thread pool |
| 啟動順序 | 必須先 build 完才能 start | Node 隨便建,spin 才開始跑 |
| 換 thread model | 換 Server 實作 | 換 Executor(Single → Multi),Node 完全不動 |
| 測試 callback | 整個 Server 起來 | 直接 `executor.spin_some()` 跑一輪退出 |

ROS 2 的 Composition(Phase 09)、LifecycleNode、MultiThreadedExecutor 全都建立在「Node 是被動」這個前提上。**業界 Nav2 把 8 個 Node 塞同一個 process 共用 thread pool — 換成 gRPC 模型根本做不到**。

> **🎓 設計通則 2**:**把「資料 / 邏輯」跟「執行 / 排程」拆開**。資料(Node)單純表達意圖,執行(Executor)負責怎麼跑。這樣使用者可以在不改邏輯的情況下換執行策略 — 是 ECS 遊戲引擎、React Concurrent Mode、Tokio Future 共通的設計。

### 決策 C:**強制 `shared_ptr<Node>` 不是龜毛,是內建約束**

下面這段**編譯不過**:

```cpp
AutoDriveNode node;          // ❌ 不能用裸物件
rclcpp::spin(&node);         // ❌ 不能傳 raw pointer
```

只能用 `std::make_shared<AutoDriveNode>()`。為什麼?

- DDS 內部對 Subscription / Publisher 持有指向 Node 的 weak reference(避免循環)
- Timer / Service callback 內部也用 weak ptr 存 Node
- `Node` 基底繼承 `enable_shared_from_this<Node>`,callback 內可以安全拿到自己

這些都需要 Node 一定要是 `shared_ptr` 管理才合法。

> **🎓 設計通則 3**:**如果你的 library 要自己管 lifecycle(callback 在 Node 死後才被觸發 = UAF),強制使用者用 smart pointer**。不要相信使用者會 manually delete 對的時機,**用編譯期約束(API 簽章)強迫他用對**。React 強制 component 是 class/function、Rust 強制 borrow checker、ROS 2 強制 shared_ptr — 都是同一個哲學。

### 決策 D:**對稱包住 — init/shutdown 必須成對,RAII 包更好**

新手寫法:

```cpp
rclcpp::init(argc, argv);
rclcpp::spin(std::make_shared<AutoDriveNode>()); // node 在這活著
rclcpp::shutdown();                              // ← 危險:node 還在,publisher 還活著時 shutdown
return 0;                                        // ← node 在這 destruct,但 Context 已關
```

業界推薦:

```cpp
rclcpp::init(argc, argv);
{
    auto node = std::make_shared<AutoDriveNode>();
    rclcpp::spin(node);
}                                                // ← node 在這 destruct,Pub/Sub 清乾淨
rclcpp::shutdown();                              // ← 此時所有 entity 都釋放完
return 0;
```

這個差別會在 Phase 13 看到 — `rclcpp_action` shutdown 順序錯就 segfault。

> 上面提到的 3 條「設計通則」歸納在 [`DESIGN_NOTES.md`](../DESIGN_NOTES.md),搭配其他主題(Subscription 的 SharedPtr 回傳、QoS 為什麼是 declarative profile、LifecycleNode 為什麼是 contract)一起看會更有體會。

---

## 🎯 學到的關鍵概念

- ROS 2 = **分散式共享發布訂閱匯流排**，沒有中央
- 通訊機制有 **3 種**：Topic（廣播）/ Service（一問一答）/ Action（長任務）
- 通訊由 **DDS** 提供（FastRTPS / CycloneDDS）——所有 ROS 2 強大的特性都來自它
- Node 之間用 **Discovery** 自動互相找到，靠 `ROS_DOMAIN_ID` 隔離
- 訊息有 **強型別**，從 .msg/IDL 定義 → 自動生成 class → CDR 序列化 → 網路傳輸
- 與 MQTT/gRPC/ROS 1 各有各的場景，別混用

---

## 下一步

學會了通訊「為什麼是這樣」。接下來深入「**怎麼用**」更多通訊機制：
- [Phase 03 — Subscriber + 光達避障](../phase-03-subscriber-lidar-brake/)：訂閱端 + QoS 對齊
- [Phase 04 — Services](../phase-04-services-toggle/)：一問一答機制
- [Phase 08 — Custom Interfaces](../phase-08-custom-interfaces/)：自己定義 .msg / .srv / .action

---

## 🔗 延伸閱讀（選讀）

- [ROS 2 設計文件 — DDS 為什麼](https://design.ros2.org/articles/ros_on_dds.html)
- [eProsima Fast DDS 官方文件](https://fast-dds.docs.eprosima.com/)
- [ROS 2 vs ROS 1 的設計取捨](https://design.ros2.org/articles/why_ros2.html)
