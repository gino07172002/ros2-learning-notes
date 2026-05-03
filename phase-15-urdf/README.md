# Phase 15：URDF + robot_state_publisher

> Part 4 第一章。從這裡開始，Node 們**有了一個身體**。

**學完你會**：寫 URDF（用 xacro 巨集系統）描述機器人物理結構、用 robot_state_publisher 把 URDF 的 link 階層自動發布成 TF tree、用 `tf2_echo` 從 CLI 驗證座標關係。

**前置**：[Phase 11 Launch 進階](../phase-11-launch-files-advanced/) — 本章需要寫 launch file。

**產出**：[`code/my_robot_description/`](code/my_robot_description/) — 完整的兩輪差速車 URDF + display launch。

**環境**：☁️ TheConstructSim + 💻 本機 WSL 雙環境通用。

---

## 為什麼要 URDF

到 Phase 14 為止你的「機器人」只是邏輯概念：fake_lidar 發訊息、smart_brake 收訊息。但**實際機器人有形狀有空間**：
- 光達裝在車頂正前方
- 兩個輪子相距 30 cm
- 攝影機朝下傾 15 度

URDF (Unified Robot Description Format) 就是描述這些**幾何關係**的標準格式。**SLAM、Nav2、MoveIt、Gazebo、RViz 全部都依賴它**。

| 你熟悉的 | URDF |
|---------|------|
| HTML/XML 描述網頁結構 | URDF 描述機器人結構 |
| CSS 樣式（顏色/尺寸） | `<visual>` 區段 |
| OOP 繼承重複物件 | xacro `<macro>` 重複 link |

---

## 🏗️ 設計：兩輪差速車

```
                lidar (圓柱，紅色)
                   │
              ┌────┴────┐
              │ base    │
   left ──────┤ link    ├──── right
   wheel       │ (藍色) │      wheel
              └─────────┘
```

4 個 link、3 個 joint：

| Link | 形狀 | 顏色 |
|------|------|------|
| `base_link` | 0.4 × 0.3 × 0.2 m 立方體 | 藍 |
| `left_wheel` / `right_wheel` | 圓柱 r=5cm 厚 4cm | 黑 |
| `lidar_link` | 圓柱 r=4cm 高 5cm | 紅 |

| Joint | 類型 | 父 → 子 |
|-------|------|---------|
| `left_wheel_joint` | continuous（無限轉） | base_link → left_wheel |
| `right_wheel_joint` | continuous | base_link → right_wheel |
| `lidar_joint` | fixed（固定） | base_link → lidar_link |

---

## 📝 URDF 用 xacro 寫

完整檔案見 [`urdf/diffbot.urdf.xacro`](code/my_robot_description/urdf/diffbot.urdf.xacro)。

### 為什麼用 xacro 而不是純 URDF

純 URDF 是 XML，**不能定義變數、不能寫巨集、不能 include**。寫個有兩個輪子的機器人就要複製貼上一大段。

xacro 是 URDF 的前處理器：

```xml
<robot name="diffbot" xmlns:xacro="http://www.ros.org/wiki/xacro">

  <!-- 定義常數 -->
  <xacro:property name="wheel_radius" value="0.05" />
  <xacro:property name="base_width" value="0.3" />

  <!-- 定義巨集（兩輪共用） -->
  <xacro:macro name="wheel" params="prefix y_offset">
    <link name="${prefix}_wheel">
      <visual>
        <geometry>
          <cylinder radius="${wheel_radius}" length="0.04" />
        </geometry>
      </visual>
    </link>

    <joint name="${prefix}_wheel_joint" type="continuous">
      <parent link="base_link" />
      <child link="${prefix}_wheel" />
      <origin xyz="0 ${y_offset} 0" rpy="-1.5708 0 0" />
      <axis xyz="0 0 1" />
    </joint>
  </xacro:macro>

  <!-- 用巨集生成兩輪 -->
  <xacro:wheel prefix="left"  y_offset="${base_width/2 + 0.02}" />
  <xacro:wheel prefix="right" y_offset="-${base_width/2 + 0.02}" />

</robot>
```

啟動時 xacro 會把巨集展開成純 URDF：

```python
# launch/display.launch.py
import xacro
robot_description = xacro.process_file(xacro_path).toxml()  # 展開
```

---

## 🦴 三種 Joint 類型（最常用）

| Type | 用途 | 例子 |
|------|------|------|
| `fixed` | 兩 link 完全鎖定 | 光達固定在車頂 |
| `continuous` | 無限旋轉 | 輪子 |
| `revolute` | 有限旋轉 | 機械臂關節 |
| `prismatic` | 直線伸縮 | 線性致動器 |

```xml
<!-- continuous: 沒 limit -->
<joint name="left_wheel_joint" type="continuous">
  <parent link="base_link" />
  <child link="left_wheel" />
  <origin xyz="0 0.17 0" rpy="-1.5708 0 0" />  <!-- 位置 + 旋轉 -->
  <axis xyz="0 0 1" />                         <!-- 旋轉軸 -->
</joint>

<!-- fixed: 不需要 axis -->
<joint name="lidar_joint" type="fixed">
  <parent link="base_link" />
  <child link="lidar_link" />
  <origin xyz="0.1 0 0.125" rpy="0 0 0" />
</joint>
```

---

## 🚀 robot_state_publisher 的角色

```
URDF text                           ┌─ /tf (動態，含輪子轉動)
   │                                │
   ▼                                │
┌─────────────────────────┐         │
│ robot_state_publisher   │ ───────▶│
│  - 解析 URDF 結構        │         │
│  - 訂閱 /joint_states    │         │
│  - 用 forward kinematics │         │
│    算每個 link 在哪      │         │
└─────────────────────────┘         │
                                    └─ /tf_static (固定，光達在車頂)
```

它是**所有 ROS 機器人系統的隱藏基礎**——RViz / SLAM / Nav2 都靠它知道 lidar 在哪、base 在哪。

---

## 🚀 Demo 流程

### Step 1：部署

#### ☁️ TheConstructSim
```bash
cd ~/ros2_ws/src
ln -s ros2-learning-notes/phase-15-urdf/code/my_robot_description .
```

#### 💻 本機 WSL2
```bash
cp -r /mnt/d/ros_learn/ros2-learning-notes/phase-15-urdf/code/my_robot_description \
      ~/ros2_ws/src/

# 確認 joint_state_publisher 套件已裝
sudo apt install -y ros-humble-joint-state-publisher
```

### Step 2：build + launch

```bash
cd ~/ros2_ws
colcon build --packages-select my_robot_description
source install/setup.bash
ros2 launch my_robot_description display.launch.py
```

預期 log：
```
[robot_state_publisher-1] got segment base_link
[robot_state_publisher-1] got segment left_wheel
[robot_state_publisher-1] got segment lidar_link
[robot_state_publisher-1] got segment right_wheel
```

### Step 3：CLI 驗證 TF（不需要 RViz）

新 terminal：

```bash
# 看所有 topic
ros2 topic list
# /joint_states
# /robot_description
# /tf
# /tf_static

# 看 base→lidar 的固定變換（驗證過）
ros2 run tf2_ros tf2_echo base_link lidar_link
```

預期輸出：
```
At time 0.0
- Translation: [0.100, 0.000, 0.125]
- Rotation: in Quaternion (xyzw) [0.000, 0.000, 0.000, 1.000]
```

🎯 **0.100 = URDF 裡寫的 `xyz="0.1 0 ..."`**，**0.125 = base_height/2 + 0.025 = 0.1 + 0.025**。從 URDF 文字到 TF 數值的完整鏈路打通。

### Step 4：產生 TF tree 圖（PDF）

```bash
ros2 run tf2_tools view_frames
# 輸出 frames.pdf 在當前資料夾
```

打開 PDF 會看到：
```
[base_link]
   ├── left_wheel
   ├── right_wheel
   └── lidar_link
```

---

## 🔍 用 RViz 看（GUI 加分）

```bash
rviz2
```

設定：
- Fixed Frame: `base_link`
- Add: `RobotModel`（從 `/robot_description`）
- Add: `TF`

你會看到藍色立方體 + 兩個黑輪子 + 一個紅光達。**這個畫面就是業界 ROS 開發者每天看的東西**。

---

## 🐛 常見雷

### 雷 1：URDF inertia 警告
```
The root link base_link has an inertia specified in the URDF, but KDL does not support a root link with an inertia.
```
這是無害警告——KDL 解析器要求 root link 沒 inertia。**production 做法**：加一個 dummy `base_footprint` link 當真正的 root，base_link 是它的子。本章為了簡單先忽略。

### 雷 2：xacro 拼字錯沒展開
```xml
<xacro:propertyy ...>   <!-- 多了一個 y -->
```
xacro 對未知 tag 會**靜默忽略**，導致變數沒定義 → 後面引用 `${name}` 報「name not found」。

### 雷 3：joint origin 設錯，TF 看起來怪
xyz/rpy 必須相對「parent link 的 origin」。常踩雷是把絕對位置寫進去。**測試方法**：用 `tf2_echo parent child` 看數值，跟你預期的 offset 對比。

### 雷 4：沒裝 joint_state_publisher
本章 launch 用到，沒裝會報：
```
ERROR: package 'joint_state_publisher' not found
```
解：`sudo apt install -y ros-humble-joint-state-publisher`

### 雷 5：xacro 用了沒 import
```python
import xacro                                        # ✅ 必須 import
robot_description = xacro.process_file(...).toxml()
```
ROS 2 的 launch file 是 Python，不會自動 import。

### 雷 6：continuous joint 不出現在 TF
連續 joint（輪子）需要有人發 `/joint_states`，否則 TF 沒值。所以本章 launch 也啟動 `joint_state_publisher`。

---

## 🎯 學到的關鍵概念

- **URDF 描述機器人物理結構**（link、joint、origin、material）
- **xacro 是 URDF 的前處理器**：屬性、巨集、include
- **三種 joint**：fixed / continuous / revolute
- **robot_state_publisher** 把 URDF + /joint_states → /tf
- **CLI 驗證**：`tf2_echo` 看單一 transform、`view_frames` 看整棵樹
- **業界基礎**：Nav2/MoveIt/Gazebo 都從 URDF 開始

---

## 🌟 進階挑戰

1. **加 base_footprint**：在 base_link 下方加一個 dummy link 當真正 root，消掉 KDL 警告
2. **xacro `<xacro:include>`**：把 wheel macro 拆到獨立 xacro 檔
3. **多機器人**：用 xacro 接受 `<xacro:arg name="robot_name">`，產生 robot1/robot2 兩台不同 namespace 的車
4. **加機械臂**：在 base 上加一個簡單的兩節 arm（base → arm_base → arm_link1 → arm_tip）

---

## 下一步

- [Phase 16 — TF2](../phase-16-tf2/)（待完成）：學會用 C++/CLI 操作 TF tree、處理時間戳、寫 broadcaster/listener

---

## 📁 完整檔案結構

```
phase-15-urdf/
├── README.md
└── code/
    └── my_robot_description/
        ├── package.xml
        ├── CMakeLists.txt
        ├── urdf/
        │   └── diffbot.urdf.xacro       ← 兩輪車 URDF
        └── launch/
            └── display.launch.py         ← 啟動 robot_state_publisher
```
