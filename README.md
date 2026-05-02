# ROS 2 學習筆記（C++）

從零開始學 ROS 2 的實戰筆記。每個 phase 是獨立的專案資料夾，自帶完整可編譯的 code 與說明，**可跳讀、可單獨執行**。

---

## 🗺️ 學什麼，看哪一章

> 章節依「**學習層次**」分成幾個 Part。每個 Part 內各章可跳讀，但 Part 之間有遞進關係——下一個 Part 假設你能用前一個 Part 的東西。

### 📖 Part 1: 通訊基礎 — 讓 Node 能講話
> 學完 Part 1 你能用現成的 ROS 訊息類型寫出 Pub/Sub/Service Node。

| Phase | 主題 | 學到什麼 | 🐍 Py |
|-------|------|----------|------|
| [01](phase-01-cloud-env-first-publisher/) | 雲端環境 + 第一支 Publisher | 建 ROS 2 套件、用 colcon 編譯、寫 Publisher 讓車子前進 | ✅ |
| [02](phase-02-communication-concepts/) | **ROS 2 設計哲學** | DDS / 節點發現 / 訊息序列化 / 與 MQTT/gRPC/ROS 1 對比 | — |
| [03](phase-03-subscriber-lidar-brake/) | Subscriber + 光達避障 | 寫 Subscriber、QoS（SensorDataQoS）、解析 PointCloud2、做避障邏輯 | ✅ |
| [04](phase-04-services-toggle/) | Service Server + 開關 | Service vs Topic、實作 SetBool 服務、雙終端機驗證 | — |

### 🔧 Part 2: 工具與治理 — 看清楚系統 + 微調系統
> 學完 Part 2 你能 debug 不熟的 ROS 系統、調整 Node 行為而不改 code，且能組合既有元件做小作品。

| Phase | 主題 | 學到什麼 | 🐍 Py |
|-------|------|----------|------|
| [05](phase-05-debug-tools/) | Debug 工具集 | rqt_graph 看通訊圖、ros2 bag 錄製/重播、rqt_plot 即時繪圖、rqt_console 集中 log | — |
| [06](phase-06-parameters/) | Parameters（參數系統） | declare/get/on_set callback、YAML 設定檔、rqt_reconfigure GUI 即時調參 | — |
| 🎯 07 | **Mini Capstone 1**：可調參數 + 遠端開關的避障車 | 整合 Phase 03/04/06，一個下午搞定 | — |

### 🏗️ Part 3: 系統設計 — 自己定義協議與架構
> **核心分水嶺**：從「使用現成 ROS 元件」進到「設計自己的 ROS 系統」。學完 Part 3 你能組起一個多節點專案。

| Phase | 主題 | 學到什麼 | 🐍 Py |
|-------|------|----------|------|
| [08](phase-08-custom-interfaces/) | Custom Interfaces（自訂訊息） | 定義 .msg / .srv / .action、interface 套件分離、rosidl 生成 C++ class | — |
| 09 | Executors / Lifecycle / Composition | （待完成） | — |
| 10 | Launch Files 基礎 | （待完成） | — |
| 11 | Launch Files 進階 | （待完成） | — |
| 12 | 測試（gtest + launch_testing） | （待完成） | — |
| 13 | Actions | （待完成） | — |
| 🎯 Capstone 1 | 整合 Action + Lifecycle + Launch + 測試（GitHub-ready） | （待完成） | — |

> 🐍 欄位：✅ 有 rclpy 對照版（資料夾內 `python/`） ｜ — 純觀念或暫無對照

完整學習路徑（含 Part 4「機器人形體」/ Part 5「領域應用」/ Part 6「生產化部署」、Track A/B 分流）見 [ROADMAP.md](ROADMAP.md)。

---

## 📁 每個 phase 的結構

```
phase-XX-topic/
├── README.md              ← 該章完整說明（觀念 + 步驟 + 程式碼）
└── code/
    └── my_cpp_pkg/        ← 完整可編譯的 ROS 2 套件
        ├── package.xml
        ├── CMakeLists.txt
        └── src/*.cpp
```

每章的 `code/my_cpp_pkg/` **都是完整獨立的套件**，不依賴前一章的 code。這意味著：
- 你可以從任何一章開始學
- 但代價是 `CMakeLists.txt` 與 `package.xml` 在各章間會重複（這是刻意的）

---

## 🛠️ 怎麼跑這些 code

本路徑支援兩種環境，**程式碼完全通用**，差異只在執行時的 topic remapping：

- ☁️ **TheConstructSim 雲端**（免裝即用，前期推薦）
- 💻 **本機 WSL2 / Ubuntu 22.04 + ROS 2 Humble**（後期專案推薦）

完整環境設定步驟、兩者比較表、各章建議的環境，見 **[SETUP.md](SETUP.md)**。

快速版：
```bash
# 兩種環境通用
cd ~/ros2_ws
colcon build --packages-select my_cpp_pkg
source install/setup.bash
ros2 run my_cpp_pkg <executable> --ros-args -r cmd_vel:=<實際topic>
```

---

## 📦 舊筆記

`_archive/` 裡是重整前的原始 markdown，保留比對用，不再維護。

---

## 🤖 給未來 AI 協作者

要寫新章節、改既有章節、加新語言版本前，**先看 [`AUTHORING_GUIDE.md`](AUTHORING_GUIDE.md)**。
裡面有資料夾結構、主/Python README 模板、程式碼風格、反模式清單與檢查表。
