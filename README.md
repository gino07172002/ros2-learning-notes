# ROS 2 學習筆記（C++）

從零開始學 ROS 2 的實戰筆記。每個 phase 是獨立的專案資料夾，自帶完整可編譯的 code 與說明，**可跳讀、可單獨執行**。

---

## 🗺️ 學什麼，看哪一章

| Phase | 主題 | 學到什麼 | 前置 |
|-------|------|----------|------|
| [01](phase-01-cloud-env-first-publisher/) | 雲端環境 + 第一支 Publisher | 建 ROS 2 套件、用 colcon 編譯、寫 Publisher 讓車子前進 | C++ 基礎、Linux terminal |
| [02](phase-02-communication-concepts/) | 通訊機制核心觀念 | Node / Topic / Message / Pub-Sub 模型，回頭拆解 Phase 01 的程式 | Phase 01 |
| [03](phase-03-subscriber-lidar-brake/) | Subscriber + 光達避障 | 寫 Subscriber、QoS（SensorDataQoS）、解析 PointCloud2、做避障邏輯 | Phase 01 |
| [04](phase-04-services-toggle/) | Service Server + 開關 | Service vs Topic、實作 SetBool 服務、雙終端機驗證 | Phase 03 |

完整學習路徑（包含尚未完成的章節）見 [ROADMAP.md](ROADMAP.md)。

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

### 雲端（TheConstructSim ROSject）
複製 `phase-XX/code/my_cpp_pkg/` 到 `~/ros2_ws/src/`，然後：
```bash
cd ~/ros2_ws
colcon build --packages-select my_cpp_pkg
source install/setup.bash
ros2 run my_cpp_pkg <executable_name>
```

### 本機（WSL2 / Ubuntu 22.04 + ROS 2 Humble）
同上。本機額外需要先 `source /opt/ros/humble/setup.bash`。

---

## 📦 舊筆記

`_archive/` 裡是重整前的原始 markdown，保留比對用，不再維護。
