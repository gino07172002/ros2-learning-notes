# Phase 19：pluginlib — Runtime 載入 C++ class

> 業界 Nav2 / MoveIt / ros2_control 全部用這個機制。學會它你就能擴充任何 ROS 系統。

**學完你會**：寫 base interface、寫具體 plugin 實作、註冊 plugins.xml、用 ClassLoader 在 runtime 載入指定 plugin。

**前置**：C++ 多型概念（virtual function、繼承）。**不需要**前面的 ROS 章節。

**產出**（三個 ROS 套件，業界標準分離）：
- [`brake_strategy_base/`](code/brake_strategy_base/) — 純抽象介面
- [`brake_strategy_plugins/`](code/brake_strategy_plugins/) — 兩個具體實作
- [`plugin_demo/`](code/plugin_demo/) — 主程式，用 ClassLoader 動態載入

**環境**：☁️ TheConstructSim + 💻 本機 WSL 雙環境通用。

---

## 為什麼這章是業界職缺剛需

業界 ROS 開發者最常被問：「**你能寫 Nav2 BT plugin 嗎？**」「**你能擴充 MoveIt planner 嗎？**」「**你能寫 ros2_control controller 嗎？**」

這三個問題的核心答案都是 **pluginlib**。

| 框架 | 你會寫 plugin → 能做什麼 |
|------|------------------------|
| **Nav2** | 自訂導航行為、避障策略、planner |
| **MoveIt** | 自訂機械臂運動規劃器 |
| **ros2_control** | 自訂硬體驅動、controller |
| **rviz2** | 自訂視覺化面板 |

**會 pluginlib = 能擴充任何 ROS 系統，不會 = 你只能用別人寫好的**。職缺薪資差距明顯。

---

## 設計：三套件分離架構（業界標準）

```
┌────────────────────────┐   定義 interface
│ brake_strategy_base    │   只有 .hpp，沒實作
│  └── BrakeStrategy.hpp │
└──────────┬─────────────┘
           │
    ┌──────┴───────┐
    │              │
┌───▼─────────┐ ┌──▼───────────────────────┐
│ plugin_demo │ │ brake_strategy_plugins   │
│ 主程式       │ │  ├── ConservativeStrategy│
│ ClassLoader │ │  └── AggressiveStrategy  │
│ → 載 plugin  │ │  └── plugins.xml         │
└─────────────┘ └──────────────────────────┘
        ↑                       ↑
        └─ runtime 用名稱 ──────┘
           載入指定 plugin
```

**為什麼分離**：
- `_base`：給所有人用的合約。**不可變**（變了所有 plugin 都要重寫）
- `_plugins`：實作。第三方可以自己另開套件實作，不需要改 _base
- `_demo`：主程式。只依賴 _base，runtime 才知道要用哪個 plugin

**Nav2 結構完全相同**：`nav2_core` (base) + `nav2_navfn_planner` / `nav2_smac_planner` (plugins) + `nav2_controller_server` (demo) + 你可以寫第四個 planner 套件加進去。

---

## 📝 三段程式碼

### 1. Base Interface（[brake_strategy_base](code/brake_strategy_base/include/brake_strategy_base/brake_strategy.hpp)）

```cpp
namespace brake_strategy_base {

class BrakeStrategy
{
public:
    // ⚠️ pluginlib 載入時用 default constructor，不能在 ctor 做事
    // 改用 initialize() 接收參數
    virtual void initialize(double safe_distance, double max_speed) = 0;

    virtual double compute_speed(double obstacle_distance) const = 0;
    virtual const char * name() const = 0;
    virtual ~BrakeStrategy() = default;
};

}
```

**重點**：純抽象 class、virtual destructor、無建構子參數。

### 2. 具體 Plugin（[brake_strategy_plugins/src](code/brake_strategy_plugins/src/)）

```cpp
// conservative_strategy.cpp
#include "brake_strategy_base/brake_strategy.hpp"
#include <pluginlib/class_list_macros.hpp>

namespace brake_strategy_plugins {

class ConservativeStrategy : public brake_strategy_base::BrakeStrategy
{
public:
    void initialize(double safe_distance, double max_speed) override { ... }

    double compute_speed(double obstacle_distance) const override {
        if (obstacle_distance < 0.0) return 0.0;
        if (obstacle_distance > safe_distance_) return max_speed_;
        return 0.0;  // 保守：直接停
    }

    const char * name() const override { return "Conservative"; }

private:
    double safe_distance_, max_speed_;
};

}

// ⚠️ 關鍵巨集：(具體 class, base class)
PLUGINLIB_EXPORT_CLASS(
    brake_strategy_plugins::ConservativeStrategy,
    brake_strategy_base::BrakeStrategy)
```

`PLUGINLIB_EXPORT_CLASS` 是 pluginlib 在 .so 內留下「我可被動態載入」的標記。

### 3. 主程式（[plugin_demo/src/strategy_loader.cpp](code/plugin_demo/src/strategy_loader.cpp)）

```cpp
#include <pluginlib/class_loader.hpp>

// ClassLoader 知道怎麼從套件名 + base class 找 plugin
auto loader = pluginlib::ClassLoader<brake_strategy_base::BrakeStrategy>(
    "brake_strategy_base",                    // 套件名
    "brake_strategy_base::BrakeStrategy");    // 完整 base class 名稱

// runtime 建出 plugin instance（用完整 class name）
auto strategy = loader.createSharedInstance(
    "brake_strategy_plugins::ConservativeStrategy");

strategy->initialize(1.0, 0.5);
double v = strategy->compute_speed(0.3);   // 呼叫到具體實作
```

**主程式不知道 ConservativeStrategy 內部怎麼寫**——它只看到 base class API。

---

## ⚙️ 三個關鍵設定檔

### plugins.xml（在 plugins 套件內）

```xml
<library path="brake_strategy_plugins">
  <class
    type="brake_strategy_plugins::ConservativeStrategy"
    base_class_type="brake_strategy_base::BrakeStrategy">
    <description>Stop completely near obstacle.</description>
  </class>
  <class
    type="brake_strategy_plugins::AggressiveStrategy"
    base_class_type="brake_strategy_base::BrakeStrategy" />
</library>
```

**`path` 是 .so 檔名（不含 lib 前綴和 .so 副檔名）**。

### CMakeLists.txt（plugins 套件）

```cmake
add_library(${PROJECT_NAME} SHARED
  src/conservative_strategy.cpp
  src/aggressive_strategy.cpp
)
ament_target_dependencies(${PROJECT_NAME}
  pluginlib brake_strategy_base
)

# ⚠️ 關鍵：把 plugins.xml 註冊給 pluginlib
pluginlib_export_plugin_description_file(brake_strategy_base plugins.xml)
```

### package.xml（plugins 套件）

```xml
<export>
  <build_type>ament_cmake</build_type>
  <!-- ⚠️ 這行讓 pluginlib 知道「本套件提供 brake_strategy_base 的 plugin」 -->
  <brake_strategy_base plugin="${prefix}/plugins.xml" />
</export>
```

**這三件事必須一致**：CMake / package.xml / plugins.xml 任一錯都會載入失敗。

---

## 🚀 Demo 流程

### Step 1：部署 + build

#### 兩種環境通用
```bash
cp -r /mnt/d/ros_learn/ros2-learning-notes/phase-19-pluginlib/code/* \
      ~/ros2_ws/src/

cd ~/ros2_ws
colcon build --packages-select brake_strategy_base brake_strategy_plugins plugin_demo
source install/setup.bash
```

> ⚠️ **編譯順序**：base → plugins → demo。colcon 自動處理依賴。

### Step 2：執行 Conservative

```bash
ros2 run plugin_demo strategy_loader Conservative
```

**預期輸出（驗證過）**：
```
[strategy_demo]: Loaded strategy: Conservative
[strategy_demo]:   obstacle=2.0m → speed=0.50m/s    ← 遠 → 全速
[strategy_demo]:   obstacle=0.8m → speed=0.00m/s    ← 近 → 直接停
[strategy_demo]:   obstacle=0.3m → speed=0.00m/s    ← 更近 → 還是停
```

### Step 3：換 Aggressive — 同主程式不同行為

```bash
ros2 run plugin_demo strategy_loader Aggressive
```

**預期輸出（驗證過）**：
```
[strategy_demo]: Loaded strategy: Aggressive
[strategy_demo]:   obstacle=2.0m → speed=0.50m/s    ← 遠 → 全速（同保守）
[strategy_demo]:   obstacle=0.8m → speed=0.15m/s    ← 近 → 30% 速度（不同！）
[strategy_demo]:   obstacle=0.3m → speed=0.15m/s    ← 更近 → 還是 30% 速度
```

🎯 **同一個 binary、傳不同 CLI、行為完全不同** — 這就是 pluginlib 的價值。

### Step 4：壞名稱看錯誤訊息

```bash
ros2 run plugin_demo strategy_loader Suicide
```

預期：
```
[ERROR] Failed to load: According to the loaded plugin descriptions the class
brake_strategy_plugins::SuicideStrategy ... does not exist.
Declared types are brake_strategy_plugins::AggressiveStrategy
                   brake_strategy_plugins::ConservativeStrategy
```

ClassLoader 會列出所有「我認得的」class——這個錯誤訊息常用來 debug「為什麼我的 plugin 載不到」。

### Step 5：第三方擴充示範

任何人都可以開新套件 `my_custom_strategy_pkg`，繼承 `BrakeStrategy`，提供自己的 `plugins.xml`。**主程式 plugin_demo 不用改、不用重 build**——直接 `ros2 run plugin_demo strategy_loader MyCustom` 就能用。

這就是為什麼 Nav2 有上百個第三方 BT node。

---

## 🐛 常見雷

### 雷 1：Plugin 載不到，沒詳細錯誤
通常是 `plugins.xml` 與 `package.xml` 內 `<export>` 標籤對不上：
```xml
<!-- package.xml -->
<export>
  <brake_strategy_base plugin="${prefix}/plugins.xml" />
  <!--  ↑ 這個名稱必須等於 base 套件名  -->
</export>
```

### 雷 2：plugins.xml 的 `path` 寫錯
```xml
<library path="brake_strategy_plugins">       <!-- ✅ 純名稱 -->
<library path="libbrake_strategy_plugins.so"> <!-- ❌ 不要前綴與副檔名 -->
```

### 雷 3：忘了 `PLUGINLIB_EXPORT_CLASS`
編譯不會錯，但 runtime ClassLoader 找不到。**每個具體 plugin .cpp 結尾都要這個巨集**。

### 雷 4：constructor 接受參數
```cpp
// ❌ pluginlib 載入時用無參 ctor 創建，給不了參數
class MyPlugin {
    MyPlugin(double x, double y) { ... }
};

// ✅ 用 initialize() pattern
class MyPlugin {
    MyPlugin() = default;
    void initialize(double x, double y) { ... }
};
```

### 雷 5：base class 沒 virtual destructor
```cpp
// ❌ 子類別 destructor 不會被呼叫 → memory leak
class Base { virtual void foo() = 0; };

// ✅
class Base {
    virtual void foo() = 0;
    virtual ~Base() = default;
};
```

### 雷 6：base 套件名 vs class 名搞錯
```cpp
// ClassLoader 第一個參數是「套件名」、第二個是「完整 class 名稱」
pluginlib::ClassLoader<Base>(
    "brake_strategy_base",                // 套件名
    "brake_strategy_base::BrakeStrategy"  // class 名（含 namespace）
);
```

---

## 🎯 學到的關鍵概念

- **三套件分離**：base（純 interface） + plugins（實作） + demo（主程式）
- **`PLUGINLIB_EXPORT_CLASS`** 巨集註冊 plugin
- **`plugins.xml`** 描述 .so 內含哪些 class
- **`pluginlib_export_plugin_description_file`** CMake 巨集註冊給 pluginlib
- **package.xml `<export>`** 也要對應宣告
- **ClassLoader + createSharedInstance** runtime 載入
- **業界 Nav2/MoveIt/ros2_control 都這個架構**

---

## 🌟 進階挑戰

1. **加第三個 plugin**：寫 `LinearStrategy`（速度線性遞減：dist=safe → 100%, dist=0 → 0%）
2. **動態切換**：寫 service 讓主程式 runtime 換 plugin（不重啟）
3. **參數從 ROS Parameter 讀**：plugin 不只接 `initialize(double, double)`，接 `initialize(rclcpp::Node *)` 自己讀 param
4. **真的擴充 Nav2**：寫一個自訂 BT node 註冊到 nav2_behavior_tree

---

## 下一步

- Phase 18 — ros2_control（待完成）：本章學的 pluginlib 是 ros2_control 的基礎
- Phase 20 — 多機通訊（待完成）

---

## 📁 完整檔案結構

```
phase-19-pluginlib/
├── README.md
└── code/
    ├── brake_strategy_base/                ← 純 header-only interface
    │   ├── package.xml
    │   ├── CMakeLists.txt
    │   └── include/brake_strategy_base/
    │       └── brake_strategy.hpp
    │
    ├── brake_strategy_plugins/             ← 兩個 plugin 實作
    │   ├── package.xml                     ← 含 <export> 註冊
    │   ├── CMakeLists.txt                  ← pluginlib_export_plugin_description_file
    │   ├── plugins.xml                     ← class 清單
    │   └── src/
    │       ├── conservative_strategy.cpp
    │       └── aggressive_strategy.cpp
    │
    └── plugin_demo/                        ← 主程式
        ├── package.xml
        ├── CMakeLists.txt
        └── src/
            └── strategy_loader.cpp
```
