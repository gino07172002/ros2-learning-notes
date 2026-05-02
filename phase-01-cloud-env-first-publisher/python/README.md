# Phase 01（Python 版）：第一支 Publisher with rclpy

**學完你會**：用 Python 的 ROS 2 客戶端函式庫 `rclpy` 寫一個 Publisher，讓模擬器中的車子前進 3 秒後停下。

**前置**：Python 基礎、Linux terminal 操作。**不需要**裝任何東西（雲端版）或已裝好 ROS 2 Humble（本機版）。

**產出**：[`my_py_pkg/`](my_py_pkg/) — 完整可執行的 ROS 2 Python 套件。

---

## 📍 課前觀念建立

如果你懂 Python 和 Linux，這樣理解 ROS 2：

1. **它是一個通訊框架**：把機器人想成一個系統，內含很多獨立的小程式（**Node**）。看影像、控制輪子、避障各是一個 Node。
2. **Node 之間靠 Topic 傳訊息**：像廣播電台。發送方不需要知道誰在聽。
3. **rclpy 是 ROS 2 的 Python API**：`rclpy.node.Node` 是基底類別，繼承它就能當 ROS Node 用。
4. **ament_python 是建構系統**：ROS 2 Python 套件用標準的 `setup.py`，配合 `colcon build` 編譯。

---

## 🛠️ 步驟 1：架設環境

### ☁️ TheConstructSim

1. 註冊 [The Construct](https://app.theconstructsim.com/) → ROSjects → Create New ROSject
2. ROS Distro 選 **ROS 2 Humble**
3. 點擊 `</> Open` 進入虛擬機

### 💻 本機 WSL2

確認已照 [SETUP.md](../../SETUP.md) 裝好 ROS 2 Humble。額外需要：

```bash
# rclpy 與 colcon 預設已含在 ros-humble-desktop，但確認一下
sudo apt install -y python3-rclpy python3-colcon-common-extensions
```

---

## 🕵️ 步驟 2：終端機偵探課

打開終端機與模擬器，先排查環境內有什麼：

```bash
# 看現有的 topic
ros2 topic list

# 看誰在發 cmd_vel、誰在訂閱
ros2 topic info /cmd_vel
```

頻道名稱會根據環境不同：
- TheConstruct OriginBot 場景：`/originbot_1/cmd_vel`
- 本機 turtlesim：`/turtle1/cmd_vel`
- 本機 turtlebot3：`/cmd_vel`

---

## 💻 步驟 3：建立 Python 套件

```bash
cd ~/ros2_ws/src
ros2 pkg create --build-type ament_python my_py_pkg --dependencies rclpy geometry_msgs
```

> 注意 `--build-type ament_python`，這是 Python 套件用的；C++ 用的是 `ament_cmake`。

<details>
<summary>💡 從 C++ 過來的讀者點這裡</summary>

`ament_python` 套件不用寫 `CMakeLists.txt`，改用 `setup.py`（標準 Python 打包格式）。差別：

| 面向 | C++ (`ament_cmake`) | Python (`ament_python`) |
|------|--------------------|------------------------|
| 編譯設定 | `CMakeLists.txt` | `setup.py` + `setup.cfg` |
| 安裝執行檔 | `install(TARGETS ...)` | `entry_points` |
| 建構速度 | 慢（要編譯） | 快（只是複製 + 註冊） |
| 執行效能 | 高 | 中（GIL 限制） |

</details>

---

## 步驟 4：撰寫 Publisher 節點

進入 `~/ros2_ws/src/my_py_pkg/my_py_pkg/`，新增 `auto_drive.py`：

> 完整檔案見 [`my_py_pkg/my_py_pkg/auto_drive.py`](my_py_pkg/my_py_pkg/auto_drive.py)

```python
import rclpy
from rclpy.node import Node
from geometry_msgs.msg import Twist


class AutoDriveNode(Node):
    def __init__(self):
        super().__init__('auto_drive_node')

        # 建立 Publisher：[訊息類別, topic 名稱, 佇列大小]
        # 用相對名稱 'cmd_vel'，執行時用 -r 動態 remap
        self.publisher = self.create_publisher(Twist, 'cmd_vel', 10)

        # 建立 Timer，每 0.5 秒呼叫一次 timer_callback
        self.timer = self.create_timer(0.5, self.timer_callback)
        self.start_time = self.get_clock().now()

    def timer_callback(self):
        msg = Twist()
        elapsed = (self.get_clock().now() - self.start_time).nanoseconds / 1e9

        if elapsed < 3.0:
            msg.linear.x = 0.2   # 前進 0.2 m/s
            msg.angular.z = 0.0
        else:
            msg.linear.x = 0.0   # 煞車
            msg.angular.z = 0.0

        self.publisher.publish(msg)


def main(args=None):
    rclpy.init(args=args)
    node = AutoDriveNode()
    rclpy.spin(node)
    node.destroy_node()
    rclpy.shutdown()


if __name__ == '__main__':
    main()
```

<details>
<summary>💡 從 C++ 過來的讀者點這裡</summary>

| C++ (rclcpp) | Python (rclpy) | 差異說明 |
|--------------|----------------|---------|
| `class : public rclcpp::Node` | `class(Node):` | Python 用標準繼承 |
| `Node("name")` | `super().__init__('name')` | 建構子呼叫不同 |
| `this->create_publisher<Twist>(...)` | `self.create_publisher(Twist, ...)` | Python 不用模板，型別當參數傳 |
| `Publisher::SharedPtr` | 直接是物件 | Python 引用語意原生支援，不用 SharedPtr |
| `std::bind(&Node::cb, this)` | `self.cb` | Python 方法本身就是 bound method |
| `create_wall_timer(500ms, ...)` | `create_timer(0.5, ...)` | Python 用秒（float）|
| `this->now()` | `self.get_clock().now()` | rclpy 沒有 Node 上的 now() shortcut |
| `rclcpp::spin(node)` | `rclpy.spin(node)` | 概念相同 |
| `rclcpp::shutdown()` | `rclpy.shutdown()` | 概念相同 |

</details>

---

## 步驟 5：設定 setup.py

打開 `~/ros2_ws/src/my_py_pkg/setup.py`，找到 `entry_points` 區塊，加入執行檔註冊：

```python
entry_points={
    'console_scripts': [
        'auto_drive = my_py_pkg.auto_drive:main',
    ],
},
```

格式是：`<指令名> = <模組路徑>:<函式名>`。

完整版見 [`my_py_pkg/setup.py`](my_py_pkg/setup.py)。

> ⚠️ 也要確認 `package.xml` 內有：
> ```xml
> <depend>rclpy</depend>
> <depend>geometry_msgs</depend>
> ```

---

## 步驟 6：編譯與執行

> 兩種環境的差異只在「remap 到哪個 topic」。完整環境比較見 [SETUP.md](../../SETUP.md)。

### ☁️ TheConstructSim

```bash
cd ~/ros2_ws
colcon build --packages-select my_py_pkg
source install/setup.bash

ros2 run my_py_pkg auto_drive --ros-args -r cmd_vel:=/originbot_1/cmd_vel
```

### 💻 本機 WSL2 + turtlesim

```bash
# Terminal 1
ros2 run turtlesim turtlesim_node

# Terminal 2
cd ~/ros2_ws
colcon build --packages-select my_py_pkg
source install/setup.bash
ros2 run my_py_pkg auto_drive --ros-args -r cmd_vel:=/turtle1/cmd_vel
```

### 💻 本機 WSL2 + turtlebot3 Gazebo

```bash
# Terminal 1
export TURTLEBOT3_MODEL=burger
ros2 launch turtlebot3_gazebo empty_world.launch.py

# Terminal 2
cd ~/ros2_ws
colcon build --packages-select my_py_pkg
source install/setup.bash
ros2 run my_py_pkg auto_drive
```

**成功指標**：模擬器中的車子（或烏龜）向前開 3 秒後停下。

---

## 🐍 Python 開發特有的提示

### 1. 改 Python code 不用重編譯

C++ 改完要重新 `colcon build`，但 Python：
```bash
# 一次性安裝（symlink 模式），之後改 code 不用重編
colcon build --packages-select my_py_pkg --symlink-install
```
之後改 `auto_drive.py` 直接 `ros2 run` 就會用新版本。**這是 Python 開發的最大優勢**。

### 2. Logger 寫法

```python
self.get_logger().info('hello')
self.get_logger().warn('warning')
self.get_logger().error('error')
```

### 3. 不要用 `print()`

`print()` 不會經過 ROS log 系統，無法被 `ros2 bag` 記錄、也不會出現在 `ros2 launch` 的整合 log。**永遠用 `self.get_logger()`**。

### 4. Ctrl+C 後可能會看到 KeyboardInterrupt

這是正常的，Python 不像 C++ 能優雅吞掉訊號。可以用 try/except 包 `rclpy.spin()`：

```python
try:
    rclpy.spin(node)
except KeyboardInterrupt:
    pass
finally:
    node.destroy_node()
    rclpy.shutdown()
```

---

## 🎯 學到的關鍵概念

- **ament_python 套件**：用 `setup.py` + `entry_points` 註冊執行檔
- **rclpy.node.Node**：所有 ROS 2 Python Node 的基底
- **Publisher 三要素**：訊息類別、Topic 名稱、佇列大小（QoS depth）
- **Timer**：用 `create_timer(period_seconds, callback)`，秒數是 float
- **rclpy.spin()**：阻塞主執行緒，讓 callback 持續運作

---

## 下一步

- C++ 主章 + 觀念補強：[../README.md](../README.md)
- 觀念深入：[Phase 02 — 通訊機制核心觀念](../../phase-02-communication-concepts/)
- Python Subscriber 進階：[Phase 03 Python 版](../../phase-03-subscriber-lidar-brake/python/)
