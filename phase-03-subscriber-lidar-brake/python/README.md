# Phase 03（Python 版）：Subscriber + 光達避障 with rclpy

**學完你會**：用 `rclpy` 寫一個既訂閱（聽光達）又發布（送速度指令）的節點，讓車子偵測到前方障礙物時自動煞車。同時掌握 ROS 2 的 QoS 概念。

**前置**：[Phase 01 Python 版](../../phase-01-cloud-env-first-publisher/python/) — 已能跑 `auto_drive`。

**產出**：[`my_py_pkg/`](my_py_pkg/) — 含 `auto_brake` 執行檔。

---

## 👁️ 核心觀念：機器人的眼睛 (LiDAR)

光達（LiDAR）發射雷射並接收反射，測量周遭距離。ROS 2 兩種常見格式：

| 格式 | 維度 | Python 處理方式 |
|------|------|---------------|
| `sensor_msgs/msg/LaserScan` | 2D | 直接迭代 `msg.ranges`（list of float） |
| `sensor_msgs/msg/PointCloud2` | 3D | 用 `sensor_msgs_py.point_cloud2.read_points` 解析 |

本章用 PointCloud2，重點在 Python 怎麼解析二進位點雲資料。

---

## 🕵️ 步驟 1：終端機偵探課（必做）

```bash
ros2 topic hz /livox/lidar     # 確認光達有在發送
ros2 topic info /livox/lidar   # 確認資料型別
```

預期 Type 是 `sensor_msgs/msg/PointCloud2`。

---

## ⚠️ 關鍵知識：QoS（Quality of Service）

ROS 2 的 Publisher 和 Subscriber 必須**達成 QoS 協議**才能成功通訊。不匹配會「靜默失敗」（沒錯誤訊息但收不到資料）。

| 場景 | 預設 QoS | 一致性 |
|------|---------|--------|
| 一般訊息（如 `cmd_vel`） | **Reliable**（可靠） | 不丟封包 |
| 感測器訊息（光達、相機） | **Best Effort**（盡力而為） | 允許丟封包，換低延遲 |

**規則**：訂閱感測器資料**必須**用 `rclpy.qos.qos_profile_sensor_data`（或自己組 QoSProfile），否則收不到。

<details>
<summary>💡 從 C++ 過來的讀者點這裡</summary>

| C++ (rclcpp) | Python (rclpy) |
|--------------|----------------|
| `rclcpp::SensorDataQoS()` | `rclpy.qos.qos_profile_sensor_data` |
| `rclcpp::QoS(10)` | `10` (整數即可) 或 `QoSProfile(depth=10)` |

Python 的 QoS 都在 `rclpy.qos` 模組下，包含 `QoSProfile`、`QoSReliabilityPolicy`、`QoSDurabilityPolicy` 等等。
</details>

---

## 💻 步驟 2：撰寫 3D 點雲自動煞車節點

**任務**：監聽 `/livox/lidar`，解析正前方 40 公分寬走廊內的點雲。最近的點 < 1.0 m 就煞車，否則維持 0.2 m/s 前進。

完整程式見 [`my_py_pkg/my_py_pkg/auto_brake.py`](my_py_pkg/my_py_pkg/auto_brake.py)：

```python
import rclpy
from rclpy.node import Node
from rclpy.qos import qos_profile_sensor_data
from geometry_msgs.msg import Twist
from sensor_msgs.msg import PointCloud2
from sensor_msgs_py import point_cloud2


class AutoBrakeNode(Node):
    def __init__(self):
        super().__init__('auto_brake_node')

        self.publisher = self.create_publisher(Twist, 'cmd_vel', 10)

        # ⚠️ 感測器一定要用 sensor_data QoS
        self.subscription = self.create_subscription(
            PointCloud2,
            'lidar_points',
            self.cloud_callback,
            qos_profile_sensor_data,
        )

        self.get_logger().info('3D Auto Brake Started!')

    def cloud_callback(self, msg: PointCloud2):
        twist = Twist()
        min_forward_distance = 100.0

        # 解析點雲：只讀 x, y 欄位
        points = point_cloud2.read_points(
            msg, field_names=('x', 'y'), skip_nans=True
        )

        # 篩選正前方 40cm 寬走廊
        for x, y in points:
            if x > 0.0 and abs(y) < 0.2:
                if x < min_forward_distance:
                    min_forward_distance = float(x)

        if min_forward_distance > 1.0:
            twist.linear.x = 0.2
            twist.angular.z = 0.0
            self.get_logger().info(
                f'Clear ahead (Closest: {min_forward_distance:.2f}m)',
                throttle_duration_sec=1.0,
            )
        else:
            twist.linear.x = 0.0
            twist.angular.z = 0.0
            self.get_logger().warn(
                f'Obstacle at {min_forward_distance:.2f}m! BRAKING!',
                throttle_duration_sec=1.0,
            )

        self.publisher.publish(twist)


def main(args=None):
    rclpy.init(args=args)
    node = AutoBrakeNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
```

<details>
<summary>💡 從 C++ 過來的讀者點這裡</summary>

| C++ (rclcpp) | Python (rclpy) | 差異 |
|--------------|----------------|------|
| `rclcpp::SensorDataQoS()` | `qos_profile_sensor_data` | Python 是預設 QoS profile 物件 |
| `PointCloud2ConstIterator<float> iter_x(*msg, "x")` | `point_cloud2.read_points(msg, field_names=('x', 'y'))` | Python 用 generator，回傳 `(x, y)` tuple |
| `RCLCPP_INFO_THROTTLE(logger, clock, 1000, "...")` | `get_logger().info(msg, throttle_duration_sec=1.0)` | Python 用關鍵字參數 |
| `std::bind(&Node::cb, this, _1)` | 直接 `self.cb` | Python 不需要 placeholder |

**效能提醒**：Python 迭代 PointCloud2 比 C++ 慢 5–10 倍。如果點雲量大（每秒 10 萬點以上），建議用 NumPy 向量化或改 C++。
</details>

---

## ⚡ NumPy 加速版（選讀）

對大型點雲，純 Python 迴圈會卡。改用 NumPy 向量化：

```python
import numpy as np

def cloud_callback(self, msg: PointCloud2):
    # 一次讀進 NumPy structured array
    points = point_cloud2.read_points_numpy(
        msg, field_names=('x', 'y'), skip_nans=True
    )
    # points 形狀: (N, 2)，N 是點數

    x = points[:, 0]
    y = points[:, 1]

    # 向量化篩選正前方 40cm 走廊
    mask = (x > 0.0) & (np.abs(y) < 0.2)
    forward_x = x[mask]

    min_forward_distance = float(forward_x.min()) if forward_x.size > 0 else 100.0
    # 後續邏輯同上...
```

實測對 10 萬點點雲：純 Python 約 80 ms，NumPy 約 3 ms。

> 注意：`read_points_numpy` 是 ROS 2 Humble 後的 API，舊版要用 `np.array(list(read_points(...)))`。

---

## 步驟 3：setup.py 與 package.xml

加入新執行檔的註冊：

```python
entry_points={
    'console_scripts': [
        'auto_drive = my_py_pkg.auto_drive:main',   # Phase 01 留下的
        'auto_brake = my_py_pkg.auto_brake:main',   # 本章新增
    ],
},
```

`package.xml` 加入新依賴：

```xml
<depend>sensor_msgs</depend>
<depend>sensor_msgs_py</depend>
```

完整版見 [`my_py_pkg/setup.py`](my_py_pkg/setup.py) 與 [`my_py_pkg/package.xml`](my_py_pkg/package.xml)。

---

## 步驟 4：編譯與執行

> 兩種環境的差異只在「remap 到哪個 topic」。完整環境比較見 [SETUP.md](../../SETUP.md)。

### ☁️ TheConstructSim（OriginBot + Livox 3D 光達）

```bash
cd ~/ros2_ws
colcon build --packages-select my_py_pkg --symlink-install
source install/setup.bash

ros2 run my_py_pkg auto_brake --ros-args \
  -r cmd_vel:=/originbot_1/cmd_vel \
  -r lidar_points:=/livox/lidar
```

### 💻 本機 WSL2（turtlebot3 waffle + 深度相機）

```bash
# Terminal 1
export TURTLEBOT3_MODEL=waffle
ros2 launch turtlebot3_gazebo turtlebot3_house.launch.py

# Terminal 2: 看實際的 PointCloud2 topic 名稱
ros2 topic list | grep -i point

# Terminal 3
cd ~/ros2_ws
colcon build --packages-select my_py_pkg --symlink-install
source install/setup.bash
ros2 run my_py_pkg auto_brake --ros-args \
  -r lidar_points:=/intel_realsense_r200_depth/points
```

---

## 🎉 成功指標

- 終端機印出車子前進時的距離資訊。
- 接近障礙物（< 1.0m）時 terminal 顯示黃色 `[WARN]`，車子煞車。

---

## 🐍 Python 開發特有提示

### 1. `--symlink-install` 是 Python 開發的好朋友

C++ 改完一定要重編。Python 用 `--symlink-install` 後，`install/` 內的 `.py` 是 symlink 指回 `src/`，**改完 source 直接 `ros2 run` 就用新版本**。

### 2. throttle log 寫法

```python
# 每秒最多印一次（rclpy 從 Iron 版開始支援）
self.get_logger().info('hello', throttle_duration_sec=1.0)
```

如果你的 rclpy 版本太舊不支援 `throttle_duration_sec`，自己用 `time.time()` 或 timer 控制：

```python
from time import time
self.last_log_time = 0.0

# in callback:
now = time()
if now - self.last_log_time > 1.0:
    self.get_logger().info('...')
    self.last_log_time = now
```

### 3. PointCloud2 解析的兩個常見坑

1. **`read_points` 是 generator，不是 list**：直接 `len()` 會錯，要先 `list(...)` 或迭代消費。
2. **`skip_nans=True` 預設關閉**：點雲常含 NaN，不過濾會在 `x > 0.0` 比較時得到 False（無害），但累積到統計算式會出錯。

### 4. 型別提示是好習慣

```python
def cloud_callback(self, msg: PointCloud2):
    ...
```

加 type hints 不只給 IDE 用，未來改 mypy 檢查也方便。ROS 2 訊息類別都是標準 Python class，可以直接 hint。

---

## 🌟 挑戰

修改 `auto_brake.py`，偵測到障礙物時讓車子轉彎（`twist.angular.z = 0.5`）而不是原地煞車，做出真正的「避障巡邏」。

---

## 🎯 學到的關鍵概念

- **rclpy 的 QoS**：`qos_profile_sensor_data` 是讀感測器的標配
- **PointCloud2 解析**：`sensor_msgs_py.point_cloud2.read_points` 取代 C++ 的 iterator
- **NumPy 向量化**：對大點雲提速 20–30 倍
- **`--symlink-install`**：Python 套件的開發模式

---

## 下一步

- C++ 主章：[../README.md](../README.md)
- 觀念補強：Phase 02 通訊機制
- 進階：Phase 04（Service Server，目前只有 C++ 版）
