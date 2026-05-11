# Phase 12：測試（gtest + rclcpp） 🚀

**學完你會**：🌟 用 gtest 寫純邏輯單元測試、寫含 rclcpp 的整合測試（驗證 publish/subscribe）、用 `colcon test` 跑整套測試、看 XML 測試報告。

**前置準備**：
- 任何 Phase 03+ 都可以——本章測試用的純邏輯是新寫的、不依賴前章

**產出目標**：
- [`src/brake_calculator.hpp`](code/my_cpp_pkg/src/brake_calculator.hpp) — 被測試的純邏輯 class
- [`test/test_brake_calculator.cpp`](code/my_cpp_pkg/test/test_brake_calculator.cpp) — 6 個單元測試
- [`test/test_with_rclcpp.cpp`](code/my_cpp_pkg/test/test_with_rclcpp.cpp) — 2 個 rclcpp 整合測試

**環境**：☁️ TheConstructSim + 💻 本機 WSL 雙環境通用。

---

## 🤔 為什麼要測試 ROS code

Phase 03–13 你寫了非常多的控制邏輯，而每次你都是靠「手動 `ros2 run` 後，用眼睛看 terminal 的 log 或 RViz 畫面」來驗證功能有沒有壞掉。但在實務上，這種做法是絕對不可行的：
- **致命的「回歸 bug」(Regression Bug)**：有時候你為了解決 A 情境的 bug，稍微改了一行判斷式，結果卻不小心讓 B 情境完全掛掉。如果沒有自動化測試，你根本不會發現 B 情境已經被你弄壞了。
- **Production 環境出包的龐大代價**：如果一個邏輯錯誤（例如遇到負值距離時沒有正確煞停）沒有在開發階段被測試抓出來，直到部署到真實的無人車上才發生，那就不只是程式當掉的問題，可能會造成實體的車損甚至嚴重危險。
- **多人協作的信任危機**：在大團隊中，別的工程師一定會去修改或重構你的程式碼。如果沒有寫好測試，他們根本不敢動你的程式碼，深怕一不小心就踩破了你預設的底線邏輯。

**自動化測試的回報是巨大的**：每次執行 `colcon test`，系統只要花 1 秒鐘就能告訴你 50 個極端情境全部 Pass，或者精準告訴你哪一行邏輯改壞了。

ROS 2 內建支援 gtest（C++）+ pytest（Python）+ launch_testing（多 Node 整合）。Nav2、MoveIt 都有完整測試套件。

---

## 測試金字塔

```
       ┌──────────────────┐
       │  E2E Tests       │  少（慢、難維護）
       │  (launch_testing)│
       ├──────────────────┤
       │  Integration     │  中（測 ROS 通訊）
       │  (gtest+rclcpp)  │
       ├──────────────────┤
       │  Unit Tests      │  多（快、好寫）
       │  (gtest only)    │
       └──────────────────┘
```

**本章重點是 Unit + Integration**。Launch_testing E2E 太重，留到 Capstone 1 時順便寫。

---

## 🧪 Test 1：純邏輯單元測試（最常用）

把純邏輯抽成 header-only class，**不依賴 rclcpp runtime**：

```cpp
// brake_calculator.hpp
class BrakeCalculator {
public:
    BrakeCalculator(double safe_distance, double slowdown_factor) ...

    double compute(double obstacle_distance, double max_speed) const {
        if (obstacle_distance < 0.0) return 0.0;
        if (obstacle_distance > safe_distance_) return max_speed;
        return max_speed * slowdown_factor_;
    }
};
```

測試檔：

```cpp
// test_brake_calculator.cpp
#include <gtest/gtest.h>
#include "../src/brake_calculator.hpp"

// 1. 沒障礙物 → 全速
TEST(BrakeCalculatorTest, NoObstacleReturnsFullSpeed) {
    BrakeCalculator calc(1.0, 0.3);
    EXPECT_DOUBLE_EQ(calc.compute(5.0, 0.5), 0.5);
}

// 2. 邊界：剛好等於 safe_distance
TEST(BrakeCalculatorTest, ExactlyAtSafeDistanceSlowsDown) {
    BrakeCalculator calc(1.0, 0.3);
    EXPECT_NEAR(calc.compute(1.0, 0.5), 0.15, 1e-9);  // = safe_distance 也算近
}

// 3. 負距離（壞資料）→ 完全停
TEST(BrakeCalculatorTest, NegativeDistanceStops) {
    BrakeCalculator calc(1.0, 0.3);
    EXPECT_DOUBLE_EQ(calc.compute(-1.0, 0.5), 0.0);
}
```

**💡 劃重點**：
- **Gtest 巨集 `TEST(SuiteName, CaseName)`**：這是 Google Test 框架的起手式。第一個參數是「測試套件名稱（通常對應你要測的 Class）」，第二個參數是「你要測的情境名稱（必須是清晰明瞭的動詞片語）」。
- **浮點數比對的致命陷阱**：寫測試時千萬別用 `==` 來比對兩個 `double`，因為浮點數在電腦運算中一定會有微小的精度誤差。請養成習慣，要求精確比對時用 `EXPECT_DOUBLE_EQ`，而允許自訂誤差範圍的比對（例如機器人跑到定位容許 1cm 誤差）則必須使用 `EXPECT_NEAR`。
- **測試必須絕對隔離**：每一個 `TEST` 區塊在執行時都是完全獨立的。A 測試的失敗，絕對不能干擾到 B 測試的執行。這確保了我們可以在一次測試中，清楚看到到底有哪些情境存活、哪些情境陣亡。

完整 6 個測試見 [`test/test_brake_calculator.cpp`](code/my_cpp_pkg/test/test_brake_calculator.cpp)。

---

## 🧪 Test 2：含 rclcpp 的整合測試

當你需要驗證「真的有訊息流動」時：

```cpp
class RclcppTestFixture : public ::testing::Test {
protected:
    void SetUp() override { rclcpp::init(0, nullptr); }
    void TearDown() override { rclcpp::shutdown(); }
};

TEST_F(RclcppTestFixture, PubSubRoundtrip) {
    auto node = std::make_shared<rclcpp::Node>("test_node");
    auto pub = node->create_publisher<std_msgs::msg::String>("test_topic", 10);

    std::string received;
    auto sub = node->create_subscription<std_msgs::msg::String>(
        "test_topic", 10,
        [&received](const std_msgs::msg::String::SharedPtr msg) {
            received = msg->data;
        });

    std::this_thread::sleep_for(500ms);  // 等 discovery

    auto msg = std_msgs::msg::String();
    msg.data = "hello test";
    pub->publish(msg);

    // spin 直到收到訊息或 timeout
    rclcpp::executors::SingleThreadedExecutor exec;
    exec.add_node(node);
    auto deadline = std::chrono::steady_clock::now() + 2s;
    while (received.empty() && std::chrono::steady_clock::now() < deadline) {
        exec.spin_some(100ms);
    }

    EXPECT_EQ(received, "hello test");
}
```

**💡 劃重點**：
- **為何改用 `TEST_F` 而不是 `TEST`？**：當你的測試需要共用複雜的初始化流程（例如啟動 ROS 節點、宣告 Publisher）時，我們不該在每個 `TEST` 裡都複製貼上一次。透過繼承 `::testing::Test` 建立 Fixture Class，然後使用 `TEST_F`，就能讓程式碼保持乾淨。
- **嚴謹的 `SetUp()` 與 `TearDown()`**：這兩個函數會在你「每一個」`TEST_F` 執行前後自動跑一次。確保每個測試情境在開始前，ROS 2 系統都被重置成乾淨狀態，結束後也能優雅地 `shutdown`，避免記憶體洩漏或節點名稱衝突。
- **DDS Discovery 的時間差**：這是整合測試中最常踩的坑！你剛建立完 Publisher 和 Subscriber 時，底層的 DDS 網路還沒完全對接。如果你立刻 `publish()`，訊息就會直接掉進黑洞。因此，必須給予 500ms 左右的等待時間，確保雙方已經互相發現 (Discovery) 後再傳送訊息。
- **彈性的 `spin_some()` 取代死板的 `spin()`**：如果在測試中直接用 `rclcpp::spin()`，程式就會永久卡死。所以我們用 `SingleThreadedExecutor::spin_some()` 搭配自訂的 Timeout 迴圈機制，如果超過兩秒都沒收到訊息，就認定測試失敗並跳出，防止整個測試流水線被卡住。

完整 2 個測試見 [`test/test_with_rclcpp.cpp`](code/my_cpp_pkg/test/test_with_rclcpp.cpp)。

---

## ⚙️ CMakeLists.txt 設定

```cmake
if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)

  # 1. 純邏輯測試
  ament_add_gtest(test_brake_calculator
    test/test_brake_calculator.cpp
  )

  # 2. 含 rclcpp 的測試
  ament_add_gtest(test_with_rclcpp
    test/test_with_rclcpp.cpp
  )
  ament_target_dependencies(test_with_rclcpp rclcpp std_msgs)
endif()
```

`package.xml`：

```xml
<test_depend>ament_cmake_gtest</test_depend>
```

> 雷：`if(BUILD_TESTING)` 不能省——它讓你 production build 時不編譯測試（CI/CD 友善）。

---

## 🚀 Demo 流程

### Step 1：部署

#### ☁️ TheConstructSim
```bash
cd ~/ros2_ws/src
ln -s ros2-learning-notes/phase-12-testing/code/my_cpp_pkg phase12_pkg
```

#### 💻 本機 WSL2
```bash
cp -r /mnt/d/ros_learn/ros2-learning-notes/phase-12-testing/code/my_cpp_pkg \
      ~/ros2_ws/src/phase12_pkg
sed -i 's|<name>my_cpp_pkg</name>|<name>phase12_pkg</name>|' ~/ros2_ws/src/phase12_pkg/package.xml
sed -i 's|project(my_cpp_pkg)|project(phase12_pkg)|' ~/ros2_ws/src/phase12_pkg/CMakeLists.txt
```

### Step 2：build + test

```bash
cd ~/ros2_ws
colcon build --packages-select phase12_pkg
colcon test --packages-select phase12_pkg
```

預期最後一行：
```
Summary: 8 tests, 0 errors, 0 failures, 0 skipped
```

### Step 3：看 XML 報告

```bash
# 簡短摘要
colcon test-result --test-result-base build/phase12_pkg/test_results

# 完整 XML 內容
find ~/ros2_ws/build/phase12_pkg/test_results -name '*.xml' -exec cat {} \;
```

實測輸出（**驗證過**）：
```xml
<testsuites tests="6" failures="0" name="AllTests">
  <testsuite name="BrakeCalculatorTest" tests="6">
    <testcase name="NoObstacleReturnsFullSpeed" />
    <testcase name="NearObstacleSlowsDown" />
    <testcase name="ExactlyAtSafeDistanceSlowsDown" />
    <testcase name="NegativeDistanceStops" />
    <testcase name="IsSlowingWhenInRange" />
    <testcase name="DifferentSlowdownFactors" />
  </testsuite>
</testsuites>

<testsuites tests="2" failures="0" name="AllTests">
  <testsuite name="RclcppTestFixture" tests="2">
    <testcase name="PubSubRoundtrip" />
    <testcase name="NodeNameCorrect" />
  </testsuite>
</testsuites>
```

CI/CD 可以解析這些 XML 自動判斷 build 過不過。

### Step 4：手動驗證單一測試

```bash
# 直接跑某個 test 看詳細輸出
~/ros2_ws/build/phase12_pkg/test_brake_calculator
```

會看到 gtest 自己的綠色 PASSED 輸出：
```
[==========] Running 6 tests from 1 test suite.
[ RUN      ] BrakeCalculatorTest.NoObstacleReturnsFullSpeed
[       OK ] BrakeCalculatorTest.NoObstacleReturnsFullSpeed (0 ms)
...
[==========] 6 tests from 1 test suite ran. (1 ms total)
[  PASSED  ] 6 tests.
```

---

## 🐛 常見雷

### 雷 1：`if(BUILD_TESTING)` 漏寫
```cmake
ament_add_gtest(...)   # ❌ production build 時也會編 test → 浪費時間
```
production CI 會用 `colcon build --cmake-args -DBUILD_TESTING=OFF`。

### 雷 2：rclcpp 測試 PubSub 收不到
DDS discovery 要時間。**必須 sleep 500ms+ 後再 publish**。

### 雷 3：rclcpp test 用 `TEST` 而非 `TEST_F`
```cpp
// ❌ 沒 fixture，每個 test 自己 init/shutdown 容易出錯
TEST(MyTest, ...)

// ✅ 用 fixture 統一管理
TEST_F(RclcppTestFixture, ...)
```

### 雷 4：`EXPECT_EQ(double, double)` 浮點誤差
```cpp
EXPECT_EQ(0.1 + 0.2, 0.3);    // ❌ FAIL！0.30000000000000004
EXPECT_DOUBLE_EQ(0.1+0.2, 0.3); // ✅ 含浮點容差
EXPECT_NEAR(0.1+0.2, 0.3, 1e-9); // ✅ 自訂容差
```

### 雷 5：rclcpp::shutdown 順序
```cpp
TearDown() override {
    // 先 shutdown context 再讓 node 析構
    rclcpp::shutdown();
}
```
順序顛倒可能 segfault。

### 雷 6：colcon test 看似成功但實際沒跑
```bash
colcon test --packages-select phase12_pkg
# Summary: 1 package finished

# 看到 finished 不代表測試 pass，要再看：
colcon test-result --test-result-base build/phase12_pkg/test_results
```

---

## 🎯 學到的關鍵概念

- **設計模式的轉變（抽離純邏輯）**：最好的 ROS 測試，就是「不要測 ROS 本身」。請把你的演算法或數學計算抽離成完全不依賴 `rclcpp` 的獨立 Class，這樣單元測試跑起來才會又快又穩定。
- **Gtest 的武器庫**：熟練運用 `TEST` (獨立測試)、`TEST_F` (有狀態測試)、`EXPECT_*` (發生錯誤但繼續執行) 以及 `ASSERT_*` (發生錯誤就立刻中斷當前測試) 等核心巨集。
- **Fixture 的重要性**：在涉及 ROS 2 API 的整合測試中，絕對要用 `SetUp/TearDown` 來嚴格管控 `rclcpp::init` 和 `rclcpp::shutdown` 的生命週期。
- **不可忽略的網路延遲 (DDS Discovery)**：任何發布與訂閱的整合測試，在建立物件連線後都必須給予適當的 `sleep` 緩衝時間，否則必定會遇到靈異的掉訊息問題。
- **完整的測試流水線三部曲**：編譯 (`colcon build`) → 執行測試 (`colcon test`) → 視覺化或解析報告 (`colcon test-result`)。
- **建構系統的防呆設計 (`BUILD_TESTING`)**：永遠記得把測試專用的 Target 包在 CMake 的 `if(BUILD_TESTING)` 區塊裡，這不但是 ROS 官方的標準規範，更能為未來的正式部署與 CI 節省可觀的編譯時間。

---

## 🌟 進階挑戰

1. **Code coverage**：用 `gcov + lcov` 看哪行 code 沒被測到
2. **launch_testing**：寫一個 .test.py 檔，啟動 smart_brake_v2 + 測試 client，驗證真的多 Node 互動
3. **Mock**：用 gmock 把 rclcpp::Publisher 換成 mock，純測試 callback 邏輯
4. **CI 整合**：寫 GitHub Actions workflow，push 觸發 colcon build + test 並 fail 整個 PR 如果有測試掛掉

---

## 👣 下一步去哪？

- [Capstone 1](../phase-14-capstone-1/)：把 Phase 09 (Lifecycle) + Phase 11 (Launch) + Phase 12 (Test) + Phase 13 (Action) 全部整合做一個 demo

---

## 📁 完整檔案結構

```
phase-12-testing/
├── README.md
└── code/
    └── my_cpp_pkg/
        ├── package.xml
        ├── CMakeLists.txt
        ├── src/
        │   └── brake_calculator.hpp        ← 被測試的純邏輯
        └── test/
            ├── test_brake_calculator.cpp   ← 6 個單元測試
            └── test_with_rclcpp.cpp        ← 2 個 rclcpp 整合測試
```
