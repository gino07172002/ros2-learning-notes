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

Phase 03–13 你寫了一堆程式，每次都靠「眼睛看 log」驗證。但實務上：
- code 改一行可能破壞另一個情境（**回歸 bug**）
- bug 在 production 才被發現代價高
- 多人協作時別人改你的 code 不知道有沒有踩到底線

**自動化測試的回報**：每次 `colcon test` 跑 1 秒就告訴你 50 個情境全 pass / 哪個 fail。

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
- `TEST(SuiteName, CaseName)` 是 gtest 巨集
- `EXPECT_DOUBLE_EQ` 完全相等、`EXPECT_NEAR` 含誤差
- **每個 TEST 獨立運作**——一個 fail 不影響其他

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
- 用 `TEST_F`（不是 `TEST`）配合 fixture class
- `SetUp()` / `TearDown()` 每個 test 都跑一次
- 必須等 DDS discovery（500ms）才能保證訊息送達
- `spin_some()` 比 `spin()` 好——可以設 timeout

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

- **抽純邏輯**：把可測試的邏輯抽成獨立 class（不依賴 rclcpp）
- **gtest 巨集**：`TEST` / `TEST_F` / `EXPECT_*` / `ASSERT_*`
- **rclcpp 測試需要 fixture**：SetUp/TearDown 管 init/shutdown
- **DDS discovery 等 500ms+**：別在 publish 前忘了 sleep
- **`colcon build` + `colcon test` + `colcon test-result`** 三步驟
- **`if(BUILD_TESTING)`** 是 ROS 標準寫法

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
