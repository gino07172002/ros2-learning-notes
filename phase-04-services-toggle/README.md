# Phase 04：Service Server + 開關

**學完你會**：實作一個 ROS 2 Service Server，可以用 CLI 從外部下達單次指令切換功能（在這裡是開關避障）。理解 Service vs Topic 的差異。

**前置**：[Phase 03](../phase-03-subscriber-lidar-brake/) — 已能跑光達避障。

**產出**：[`code/my_cpp_pkg/`](code/my_cpp_pkg/) — 含 `auto_brake_service` 執行檔,同時是 Subscriber + Publisher + Service Server。

**環境**:☁️ TheConstructSim + 💻 本機 WSL 雙環境通用。本機需要 PointCloud2(用 `~/fake_lidar.py` 或 turtlebot3 waffle)。

---

## 🧠 核心觀念：Topic vs Service

| 模式 | 比喻 | 適合的場景 |
|------|------|-----------|
| **Topic（發布/訂閱）** | 廣播電台。沒人聽也持續播。 | 感測器資料、連續控制指令 |
| **Service（一問一答）** | 打電話點餐。有請求才有回應。 | 開關功能、單次計算、拍照 |

Service 包含兩個角色：
- **Server（提供者）**：接電話的人。收到 Request → 處理 → 回 Response。
- **Client（呼叫者）**：打電話的人。送 Request → 等 Response。

本章把 Phase 03 的避障改造為 Service Server,提供外部開關。

---

## 🕵️ 終端機偵探課:看 ROS 2 預設有哪些 service

在自己寫 service 之前,**先看 ROS 系統原本有什麼**。任何 ROS 2 Node 啟動後都自帶內建 service,用 CLI 看一下:

```bash
# 啟動 Phase 03 的 auto_brake(任何 ROS 2 Node 都行)
ros2 run my_cpp_pkg auto_brake &

# 看這個 Node 提供哪些 service
ros2 service list
```

預期看到:
```
/auto_brake_node/describe_parameters       ← 內建 6 個 parameter 相關
/auto_brake_node/get_parameter_types
/auto_brake_node/get_parameters
/auto_brake_node/list_parameters
/auto_brake_node/set_parameters
/auto_brake_node/set_parameters_atomically
```

**重點**:
- 每個 ROS 2 Node 啟動就**自動有 6 個 parameter service**(用 namespace `<node_name>/...`)— Phase 06 會深入用
- 想看 service 是什麼型別 → `ros2 service type /auto_brake_node/get_parameters`
- 想直接呼叫看看 → `ros2 service call <service> <type> "<yaml>"`

```bash
# 例:查 auto_brake_node 有哪些 parameter(目前還沒設,清單會空)
ros2 service call /auto_brake_node/list_parameters \
  rcl_interfaces/srv/ListParameters
```

**這章要做的事**:加一個 `/toggle_brake` service(自訂名稱),讓外部可以開關避障。學會了下面的步驟,你就會自己寫 service。

---

## 💻 步驟 1:撰寫帶 Service 的節點

使用 ROS 2 內建的 `std_srvs/srv/SetBool`：Request 是布林、Response 是 `success`+`message`。

完整程式見 [`code/my_cpp_pkg/src/auto_brake_service.cpp`](code/my_cpp_pkg/src/auto_brake_service.cpp)：

```cpp
#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/twist.hpp"
#include "sensor_msgs/msg/point_cloud2.hpp"
#include "sensor_msgs/point_cloud2_iterator.hpp"
#include "std_srvs/srv/set_bool.hpp"
#include <atomic>
#include <cmath>

using std::placeholders::_1;
using std::placeholders::_2;

class AutoBrakeServiceNode : public rclcpp::Node
{
public:
    AutoBrakeServiceNode() : Node("auto_brake_service_node")
    {
        publisher_ = this->create_publisher<geometry_msgs::msg::Twist>("cmd_vel", 10);

        subscriber_ = this->create_subscription<sensor_msgs::msg::PointCloud2>(
            "lidar_points", rclcpp::SensorDataQoS(),
            std::bind(&AutoBrakeServiceNode::cloud_callback, this, _1));

        // 建立 Service Server
        service_ = this->create_service<std_srvs::srv::SetBool>(
            "toggle_brake",
            std::bind(&AutoBrakeServiceNode::toggle_brake_callback, this, _1, _2));

        RCLCPP_INFO(this->get_logger(), "AEB Service ready: 'toggle_brake'");
    }

private:
    // ⚠️ 用 atomic：之後切到 MultiThreadedExecutor 時，兩個 callback 可能在不同 thread
    std::atomic<bool> is_brake_active_{true};

    void toggle_brake_callback(
        const std::shared_ptr<std_srvs::srv::SetBool::Request> request,
        std::shared_ptr<std_srvs::srv::SetBool::Response> response)
    {
        is_brake_active_ = request->data;
        response->success = true;
        response->message = is_brake_active_
            ? "Brake system ENABLED."
            : "Brake system DISABLED. Watch out!";
        RCLCPP_WARN(this->get_logger(), ">>> Service: %s <<<",
                    is_brake_active_ ? "ENABLED" : "DISABLED");
    }

    void cloud_callback(const sensor_msgs::msg::PointCloud2::SharedPtr msg)
    {
        if (!is_brake_active_) {
            RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 2000,
                "Brake system offline. Waiting for enable command...");
            return;
        }

        auto twist_msg = geometry_msgs::msg::Twist();
        float min_forward_distance = 100.0f;
        sensor_msgs::PointCloud2ConstIterator<float> iter_x(*msg, "x");
        sensor_msgs::PointCloud2ConstIterator<float> iter_y(*msg, "y");

        for (; iter_x != iter_x.end(); ++iter_x, ++iter_y) {
            float x = *iter_x;
            float y = *iter_y;
            if (x > 0.0f && std::abs(y) < 0.2f) {
                if (x < min_forward_distance) min_forward_distance = x;
            }
        }

        if (min_forward_distance > 1.0f) {
            twist_msg.linear.x = 0.2;
            twist_msg.angular.z = 0.0;
        } else {
            twist_msg.linear.x = 0.0;
            twist_msg.angular.z = 0.0;
            RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 1000,
                "Obstacle detected at %.2fm! BRAKING!", min_forward_distance);
        }
        publisher_->publish(twist_msg);
    }

    rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr publisher_;
    rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::SharedPtr subscriber_;
    rclcpp::Service<std_srvs::srv::SetBool>::SharedPtr service_;
};

int main(int argc, char * argv[])
{
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<AutoBrakeServiceNode>());
    rclcpp::shutdown();
    return 0;
}
```

> **與原筆記的差異**：
> 1. Topic 用相對名稱（`cmd_vel`、`lidar_points`），Service 也用相對名稱（`toggle_brake`）。沒有 namespace 時，執行後會看到 `/cmd_vel`、`/lidar_points`、`/toggle_brake`。
> 2. `is_brake_active_` 改成 `std::atomic<bool>`。**為什麼**：預設 `SingleThreadedExecutor` 通常不會 race，但之後如果切到 `MultiThreadedExecutor`，service callback 與 sub callback 可能在不同 thread，普通 `bool` 會造成 race condition。這裡先養成共享狀態用 `atomic` 或 mutex 的習慣。

---

## 📦 步驟 2：CMakeLists.txt 與 package.xml

完整版見 [`code/my_cpp_pkg/CMakeLists.txt`](code/my_cpp_pkg/CMakeLists.txt) 與 [`code/my_cpp_pkg/package.xml`](code/my_cpp_pkg/package.xml)。

關鍵新增：

```cmake
find_package(std_srvs REQUIRED)

add_executable(auto_brake_service src/auto_brake_service.cpp)
ament_target_dependencies(auto_brake_service
  rclcpp geometry_msgs sensor_msgs std_srvs)

install(TARGETS
  auto_brake_service
  DESTINATION lib/${PROJECT_NAME}
)
```

```xml
<depend>std_srvs</depend>
```

> `install(TARGETS ...)` 很重要：`colcon build` 後，`ros2 run my_cpp_pkg auto_brake_service` 是從 install space 找執行檔。少這段常見症狀是 build 成功，但 `ros2 run` 找不到 executable。

---

## 🚀 步驟 3:編譯與實戰測試

> 兩種環境的差異只在「remap 到哪個 topic」。完整環境比較見 [SETUP.md](../SETUP.md)。

### 部署 + 編譯(兩種環境通用)

> 詳細的「workspace 從零建立」步驟見 [Phase 03 Step 6](../phase-03-subscriber-lidar-brake/)。本章只列重點:

```bash
# 部署套件
cp -r code/my_cpp_pkg ~/ros2_ws/src/    # ← 如果 my_cpp_pkg 已存在,改用合併:
                                         #   只把本章的 src/auto_brake_service.cpp 加進去,
                                         #   並更新 CMakeLists.txt + package.xml(別直接覆蓋)

# 編譯
cd ~/ros2_ws
colcon build --packages-select my_cpp_pkg
source install/setup.bash
```

> 💡 每開新 terminal 都要先 `cd ~/ros2_ws && source install/setup.bash`,不然 `ros2 run` 找不到套件。

### ☁️ TheConstructSim：啟動 Server

```bash
ros2 run my_cpp_pkg auto_brake_service --ros-args \
  -r cmd_vel:=/originbot_1/cmd_vel \
  -r lidar_points:=/livox/lidar
```

### 💻 本機 WSL2:準備 PointCloud2,再啟動 Server

本機 TurtleBot3 burger 預設只有 2D LaserScan(`/scan`),不是本章需要的 PointCloud2。需要靠 `~/fake_lidar.py` 製造假光達,所以本機會多開一個 terminal:

**Terminal 1:發假光達**

```bash
python3 ~/fake_lidar.py 0.5
```

**Terminal 2:啟動 Server**

```bash
# fake_lidar.py 預設發到 /lidar_points,不需要 remap lidar_points
ros2 run my_cpp_pkg auto_brake_service
```

如果你是用 Phase 03 的 TurtleBot3 waffle + Gazebo RealSense 做法,改用這個啟動指令:

```bash
ros2 run my_cpp_pkg auto_brake_service --ros-args \
  -r lidar_points:=/intel_realsense_r200_depth/points
```

---

### 🔧 呼叫 Service 開關避障(雲端 / 本機通用)

> 這步**雲端跟本機都要做**。雲端是另開一個 terminal,本機是第 3 個 terminal。

先確認 service 有被註冊出來:

```bash
ros2 service list | grep toggle_brake
ros2 service type /toggle_brake
```

預期看到 `/toggle_brake`,型別是 `std_srvs/srv/SetBool`。接著呼叫:

```bash
# 關閉避障
ros2 service call /toggle_brake std_srvs/srv/SetBool "{data: false}"

# 重新啟動避障
ros2 service call /toggle_brake std_srvs/srv/SetBool "{data: true}"
```

> ⚠️ **注意**:這裡的「關閉避障」是讓本節點**停止處理光達並停止發布新的 `cmd_vel`**,不是送出一筆停車命令。如果前一筆速度命令還被下游控制器短暫保留,機器人可能不會立刻停住。真機安全邏輯通常會另外設計速度 timeout 或 emergency stop。

> 💡 進階:用 `rqt_service_caller`(GUI 版)也可以呼叫,Phase 05 會教。

---

## 📊 系統日誌解讀

成功後你會看到兩段對比清楚展示 Topic(持續)vs Service(突發)。

> 下面截圖使用本機 `fake_lidar.py 0.5` 示範,所以會固定看到 0.50m 障礙物。TheConstructSim 連真模擬光達時,距離數字會依場景變化。

### 情境 1：正常避障（System ENABLED）

![兩個 terminal 對照：上方 fake_lidar 在發 PointCloud2，下方 auto_brake_service 持續輸出 BRAKING](images/state_braking.png)

> 上方 Terminal 1 是 `fake_lidar.py`，每 0.1 秒往 `/lidar_points` 發一筆「障礙物在 0.5m」的 PointCloud2。
> 下方 Terminal 2 是 `auto_brake_service`，每秒 throttle 出一筆 `BRAKING` warning。`is_brake_active_` = true，光達持續觸發 callback，發送速度 0.0。

```
[WARN] [auto_brake_service_node]: Obstacle detected at 0.50m! BRAKING!
[WARN] [auto_brake_service_node]: Obstacle detected at 0.50m! BRAKING!
```

### 情境 2 & 3：Service 呼叫瞬間 + 進入休眠

![完整故事：上半 Terminal 2 的 BRAKING → DISABLED → offline 三段轉換，下半 Terminal 3 的 service call 與 Response](images/service_toggle_full.png)

> 一張圖看完整個 Service 通訊故事——
>
> **下半 Terminal 3** 跑 `ros2 service call /toggle_brake std_srvs/srv/SetBool "{data: false}"`：
> - `requester: making request: SetBool_Request(data=False)` ← Client 端送 request
> - `response: SetBool_Response(success=True, message='Brake system DISABLED. Watch out!')` ← Server 回 response
>
> **上半 Terminal 2** 同步看到狀態切換：
> - 一連串 `BRAKING`（System ENABLED 期間）
> - 中斷一行 `>>> Service: DISABLED <<<`（toggle_brake_callback 被觸發那瞬間）
> - 之後變成 `Brake system offline. Waiting for enable command...`（cloud_callback 還在被光達觸發，但因為 `is_brake_active_=false` 直接 return）

訊息範例：
```
[WARN] >>> Service: DISABLED <<<                                ← 攔截到 service call
[INFO] Brake system offline. Waiting for enable command...      ← 進入休眠
[INFO] Brake system offline. Waiting for enable command...
```

---

## 🎬 Service vs Topic 的對照(從 demo 看懂)

跑完上面 demo,你現在能體會章首觀念表「廣播電台 vs 打電話」**具體在 callback 觸發頻率上的差異**:

| 維度 | Topic(光達 PointCloud2) | Service(toggle_brake) |
|------|------------------------|----------------------|
| Callback 觸發頻率 | **持續**(10 Hz)— 即使休眠狀態也照常觸發 | **瞬間**(只在呼叫那一刻一次) |
| 訂閱者數量 | 多對多(N publisher、N subscriber) | 一對一(每次呼叫一個 server) |
| 是否阻塞 caller | Publisher 不等(async) | Client `service.call()` 等 response(sync) |
| 適合什麼 | 連續資料流、感測器、命令 | 開關、查詢、單次計算 |

**重點觀察**:
- 即使 `is_brake_active_ = false`,`cloud_callback` **還是被光達觸發 10 Hz**(只是進去馬上 return)。Topic 訂閱永不停止,**這是 ROS 設計**
- `toggle_brake_callback` **只在 `ros2 service call` 那一瞬間**執行一次
- 想要「停止接收 Topic」必須**摧毀 subscription**(本章沒做),不是改 flag

這個觀察是 Phase 09 學 `MultiThreadedExecutor` + `CallbackGroup` 的伏筆 — Topic 不停打,你要怎麼避免它擋住 Service。

---

## 🎯 學到的關鍵概念

- **Service = Request-Response 一問一答**，與 Topic 的廣播模式對立
- **Service callback 簽章**：`(Request, Response)`，回傳值寫進 `Response`
- **多 callback 共享狀態要小心**：用 `std::atomic` 或 mutex
- **CLI 呼叫格式**：`ros2 service call <name> <type> "<data_yaml>"`

---

## 下一步

- [Phase 05 — Debug 工具集](../phase-05-debug-tools/):用 `rqt_graph` 視覺化看通訊圖、`ros2 bag` 錄重播、`rqt_service_caller` GUI 呼叫 service(本章你用 CLI 呼叫 `/toggle_brake`,Phase 05 教 GUI 版)
- [Phase 06 — Parameters](../phase-06-parameters/):讓常數(`0.2 m/s`、`1.0 m`)變外部動態調整,免重新編譯
