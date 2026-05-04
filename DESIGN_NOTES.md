# 設計筆記:把 ROS 2 當成 library 設計教材讀

> 一份「**為什麼 ROS 2 這樣設計**」的深挖筆記。
> 學完之後你不只會用 ROS 2,還能帶走可移植到其他專案的 **library 設計通則**。

---

## ⚠️ 來源分級(必讀)

寫「設計理由」最大的陷阱是**講錯**。本文件對每個論述標明信心等級:

| 標記 | 意思 |
|------|------|
| 📖 **官方記錄** | 引用 [design.ros2.org](https://design.ros2.org)、REP、設計討論串 |
| 🏭 **業界共識** | 大家都這樣解釋,但官方文件可能不直接寫(例:強制 shared_ptr 因 DDS weak ref) |
| 💭 **我的合理推測** | 從 code 結構推出來的設計動機,**可能跟官方意圖有出入** |

讀的時候請依等級判斷,不要把 💭 當官方說法引用出去。

---

## 📚 主題目錄

| # | 主題 | 對應 ROS 2 概念 | 學到的設計通則 |
|---|------|----------------|--------------|
| **01** | [init / spin / shutdown 三段式](#01-initspinshutdown-三段式) | rclcpp 入口 | 全域 runtime / 邏輯 vs 執行分離 / 強制 smart pointer |
| 02 | (待寫) Subscription / Publisher 為什麼回傳 `SharedPtr` | rclcpp::create_publisher | RAII 物件 vs handle 的權衡 |
| 03 | (待寫) QoS 為什麼是 declarative profile | rclcpp::QoS | 設定的 declarative vs imperative |
| 04 | (待寫) LifecycleNode 是 contract 不是 init 函式 | rclcpp_lifecycle | 狀態機 API 設計 |
| 05 | (待寫) pluginlib 三檔對齊的設計 | pluginlib | runtime 動態載入的安全機制 |
| 06 | (待寫) 為什麼 Topic 不寫死要用 remap | --ros-args -r | 推遲綁定(late binding)|

---

## 01. init / spin / shutdown 三段式

```cpp
rclcpp::init(argc, argv);                        // (1)
rclcpp::spin(std::make_shared<AutoDriveNode>()); // (2)
rclcpp::shutdown();                              // (3)
```

這三行是寫 ROS 2 C++ 程式的標準入口。看似簡單,藏著 **4 個設計決策** + **3 條可帶走的 library 設計通則**。

> **完整入門講解**(API 是什麼、怎麼用)在 [Phase 02 設計哲學章](phase-02-communication-concepts/README.md#-設計哲學為什麼是-init--spin--shutdown-三段式)。本節是**長版深挖**,假設你已讀過 Phase 02。

---

### 設計決策 A:全域 Context 模型 — 不是每個 Node 各自一個

#### 表面行為

`rclcpp::init` 不是初始化 Node,是**初始化整個 process 的 ROS 2 Context**:

🏭 業界共識
- 接管 SIGINT(Ctrl+C → 優雅 shutdown,而非 kill -9)
- 啟動背景 DDS Discovery thread
- 解析 `--ros-args`(remap、param 從這裡進來)
- 初始化全域 logger
- **每個 process 只能呼叫一次**(內部 atomic flag,再呼叫會 throw `ContextAlreadyInitialized`)

📖 官方:Context 概念詳見 [REP 2000](https://www.ros.org/reps/rep-2000.html) 與 [rclcpp::Context API](https://docs.ros2.org/latest/api/rclcpp/classrclcpp_1_1Context.html)。

#### 跟你熟的東西對比

| 框架 | 等價的「全域初始化」 | 是否拿走 main 控制權 |
|------|-------------------|------------------|
| Python `logging` | `logging.basicConfig()` | ❌ 不拿 |
| ROS 2 | `rclcpp::init(argc, argv)` | ✅ 拿(SIGINT、argv)|
| MPI | `MPI_Init(&argc, &argv)` | ✅ 拿(rank 注入)|
| Qt | `QApplication app(argc, argv)` | ✅ 拿(event loop) |
| OpenGL/GLFW | `glfwInit()` | ✅ 部分(window context)|

ROS 2 是**framework-style**:它預設你的 `main` 是「**ROS 2 application 的入口**」而不是「**借用 ROS 2 的程式**」。這就是為什麼可以無痛接管 SIGINT 跟 argv。

#### 設計動機(💭 我的推測)

為什麼不用 static initialization(C++ 老派做法)?

```cpp
// 假想 — ROS 2 沒這樣做的版本
namespace rclcpp {
    static Context global_context;   // process 啟動就 init
}
```

問題:
1. **跨 translation unit 的 static init 順序是 undefined** — context 可能在 logger 之前/之後初始化,難 debug
2. **拿不到 argv** — main 還沒跑,argc/argv 不存在,`--ros-args` 無解
3. **單元測試不能 mock** — 想換假 context 沒 hook 點
4. **多 thread 啟動 race** — 第一個訪問 static 的 thread 可能還沒 init 完

顯式 `init()` 全部解掉。

---

### 🎓 通則 1:**有全域狀態時,用顯式 `init()` 不要藏在 static 初始化**

**適用場景**:
- 你的 library 需要 thread / signal handler / 解析命令列
- 使用者可能想在測試裡換 mock 版本
- 多個物件共享 context

**反模式**:把 `Context global_ctx;` 放 namespace scope,期望 process 啟動自動 init。

**正確做法**:
```cpp
// 你的 library 設計範本
namespace mylib {
    void init(int argc, char** argv);    // 顯式
    void shutdown();                      // 對稱
    bool is_initialized();                // 可查詢

    // 進階:接受 InitOptions 給單元測試 mock
    void init(int argc, char** argv, const InitOptions& opts = {});
}
```

**業界範例**:Boost.Log、spdlog、gRPC `grpc_init()`、glfw、MPI、Qt — 全部都用顯式 init。

---

### 設計決策 B:Node 是被動資料結構,Executor 才是執行單位

#### 表面行為

🏭 跟 gRPC / Tornado / Express 全部不一樣。看 code 對比:

```cpp
// gRPC C++:Server 自己跑
grpc::ServerBuilder builder;
auto server = builder.BuildAndStart();
server->Wait();                     // ← Server 物件 = 執行單位
```

```cpp
// ROS 2:Node 不會自己跑
auto node = std::make_shared<AutoDriveNode>();
rclcpp::spin(node);                 // ← 外部 Executor = 執行單位
```

📖 官方理由:[Executor design article](https://design.ros2.org/articles/executor.html) 明確分開「Node = 邏輯持有者」「Executor = 執行排程」。

#### 帶來的彈性

| 場景 | gRPC 模型 | ROS 2 模型 |
|------|----------|-----------|
| 同 process 跑 N 個服務 | N 個 Server + N 個 thread | 1 Executor 共用 thread pool,N 個 Node |
| 啟動順序 | 必須 build 完才 start | Node 隨便建,spin 才跑 |
| 換 thread model | 換 Server 實作 | 換 Executor(Single → Multi),Node 完全不動 |
| 測試 callback | 整個 Server 起來 | `executor.spin_some()` 跑一輪退出 |
| Lifecycle 切換 | 銷毀重建 Server | Node 內部狀態切,Executor 不動 |

ROS 2 的 **Composition**(Nav2 把 8 個 Node 塞同 process)、**LifecycleNode**(Phase 09)、**MultiThreadedExecutor + CallbackGroup**(Phase 09)全都建立在這個分離上。

#### 設計動機(🏭 業界共識)

機器人系統**節點數量遠大於一般後端服務**。一個自駕車有 50+ 節點。如果每個都自己一個 thread:
- 50 個 thread,context switch 成本爆炸
- 想做 zero-copy IPC(Phase 09 Composition)沒辦法 — thread 不共享 heap 視角
- Lifecycle 控制變成 OS 層級的 process / thread 操作,而不是物件方法

把「Node = 邏輯」「Executor = 排程」拆開,**完全在 user-space 解決**這些問題。

---

### 🎓 通則 2:**邏輯 / 執行分離 — 把「想做什麼」跟「怎麼跑」拆開**

**適用場景**:
- 使用者可能要在不改邏輯下換執行策略(thread 數、優先序、批次大小)
- 同一個邏輯物件需要支援多種執行模式(同步測試 vs 異步生產)
- 有大量同類物件要共享資源

**反模式**:Server class 內部開 thread + 跑 loop,使用者沒辦法改。

**正確做法**:
```cpp
// 邏輯
class MyNode {
    void on_message(...);     // 純邏輯,不知道誰呼叫它
};

// 執行(可換)
class Executor {
    void add(std::shared_ptr<MyNode>);
    void spin();              // 怎麼跑由我決定
};
```

**業界範例**:
- ECS 遊戲引擎:Entity = 資料,System = 執行
- React Concurrent Mode:component = 邏輯,scheduler = 怎麼 render
- Tokio Future:Future = 邏輯,Runtime = 怎麼 poll
- Linux kqueue / epoll:fd = 資料,event loop = 排程

---

### 設計決策 C:強制 `shared_ptr<Node>` 不是龜毛,是內建約束

#### 表面行為

下面這段**編譯不過**:

```cpp
AutoDriveNode node;          // ❌ 不能用裸物件
rclcpp::spin(&node);         // ❌ 不能傳 raw pointer
```

只能 `std::make_shared<AutoDriveNode>()`。

#### 設計動機(🏭 業界共識)

ROS 2 內部到處需要「**callback 觸發時 Node 還活著**」這個保證:

```cpp
// 簡化版的 rclcpp 內部
class Subscription {
    std::weak_ptr<Node> node_;        // ← 用 weak 避免循環
    void on_message(...) {
        if (auto n = node_.lock()) {  // ← 沒撐住 = Node 已死,跳過
            // 安全呼叫 callback
        }
    }
};
```

如果 Node 是裸物件,**沒有 weak_ptr 機制能偵測它已被 destruct**,就會 use-after-free。

📖 設計細節:`Node` 繼承 `std::enable_shared_from_this<Node>`,callback 內 `shared_from_this()` 可以拿自己的 shared_ptr。

#### 為什麼不是 raw pointer + 文件警告?

C++ 的核心哲學:**若一定要對的用法,用編譯期約束逼使用者照做,不要靠文件**。

```cpp
// ❌ 失敗的 API 設計 — 註解叫人別這樣用,但編譯能過
void spin(Node* node);   // "WARNING: node must outlive spin()"

// ✅ 強制使用者一定要 shared_ptr — 用錯就編譯不過
void spin(std::shared_ptr<Node> node);
```

---

### 🎓 通則 3:**Library 內部要管 lifecycle 的話,用 API 簽章強迫使用者用 smart pointer**

**適用場景**:
- callback / 非同步事件:可能在物件 destruct 後才觸發
- 物件被多個內部子系統引用
- 跨 thread 傳遞物件 ownership

**反模式**:在文件寫「請保證物件在 X 結束前不要 destruct」。沒人會看,沒人會記。

**正確做法**:
```cpp
// 強制 shared ownership
void register_callback(std::shared_ptr<Listener>);

// 或更嚴格 — 用 unique_ptr 強制 transfer ownership
void take_over(std::unique_ptr<Listener>);
```

**業界範例**:
- React 強制 component 是 class/function(不能是裸物件)
- Rust borrow checker:編譯期保證沒 dangling reference
- Tokio:`spawn` 要求 `'static` lifetime,逼你用 owned types

---

### 設計決策 D:RAII 對稱 — init / shutdown 必須包住 spin

#### 新手 vs 業界寫法

```cpp
// ❌ 新手寫法 — Phase 13 會 segfault
rclcpp::init(argc, argv);
rclcpp::spin(std::make_shared<AutoDriveNode>());  // node 在這活著
rclcpp::shutdown();                                // ← 危險!node 跟 publisher 還活著
return 0;
```

```cpp
// ✅ 業界寫法 — RAII 對稱
rclcpp::init(argc, argv);
{
    auto node = std::make_shared<AutoDriveNode>();
    rclcpp::spin(node);
}                                                  // ← node destruct,Pub/Sub 清乾淨
rclcpp::shutdown();                                // ← 此時所有 entity 都釋放完
return 0;
```

#### 為什麼差異會 segfault?

🏭 ROS 2 entity(Publisher / Subscription / Service)在 destruct 時要回呼 DDS 通知它離開。如果 Context 已經 shutdown,**DDS 已經處於 cleanup 狀態,二度 cleanup → 未定義行為**。

Phase 13 README 內就寫了 `rclcpp_action::Client` 的 segfault 雷,根因就是這個順序問題。

---

> 上面 3 條通則本身可單獨成立,任一條套到你的 library 都會比較好用。但**最強的還是組合起來** — ROS 2 之所以能撐起 50+ 節點 / 業界自駕車這種規模,是 3 條一起用才有的綜合效益。

---

## 02. (待寫) Subscription / Publisher 為什麼回傳 SharedPtr

> 之後補。預計講:RAII handle vs callback registration 的權衡、為什麼 Subscription 是 SharedPtr 而 Publisher 也是,而不是回傳 unique_ptr 或 ID。

## 03. (待寫) QoS 為什麼是 declarative profile

> 之後補。預計講:`rclcpp::QoS(10).reliable().transient_local()` 看似 builder pattern,為什麼不是 setter chain;預設 profile(SensorDataQoS、SystemDefaultQoS)的設計考量。

## 04. (待寫) LifecycleNode 是 contract 不是 init 函式

> 之後補。預計講:5 狀態機為什麼是 unconfigured/inactive/active/finalized/error,而不是「init / start / stop」三函式;`on_configure` 必須回 SUCCESS/FAILURE 的設計。

## 05. (待寫) pluginlib 三檔對齊的設計

> 之後補。預計講:plugins.xml + CMakeLists 的 `pluginlib_export_plugin_description_file` + package.xml 的 `<export>` 為什麼非要三檔同步,看似冗餘的設計怎麼防住「runtime dlopen 抓錯版本」這個業界毒瘤。

## 06. (待寫) 為什麼 Topic 不寫死要用 remap

> 之後補。預計講:late binding 的設計哲學、為什麼相對名 + 啟動時 remap 比寫死省事一萬倍、與 Unix 哲學「pipeline 不知道對方是誰」的對應。

---

## 🔗 延伸閱讀

- [design.ros2.org](https://design.ros2.org/) — ROS 2 官方設計文件,所有重大設計都有 article
- [REP 2000](https://www.ros.org/reps/rep-2000.html) — ROS Enhancement Proposals 內 ROS 2 系列
- [rclcpp API docs](https://docs.ros2.org/latest/api/rclcpp/) — 看 Context / Executor / Node 的 source 註解,裡面常常寫 design rationale

---

## 📝 想看別的主題?

DESIGN_NOTES.md 列的「待寫」主題不是固定清單,**遇到 ROS 2 哪個設計讓你「為什麼這樣?」就可以加進來**。歡迎開 issue 提議。
