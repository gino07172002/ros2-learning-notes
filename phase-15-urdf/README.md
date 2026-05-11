# Phase 15：URDF + robot_state_publisher 🚀

> Part 4 第一章。從這裡開始，Node 們**有了一個身體**。

**學完你會**：🌟 寫 URDF（用 xacro 巨集系統）描述機器人物理結構、用 robot_state_publisher 把 URDF 的 link 階層自動發布成 TF tree、用 `tf2_echo` 從 CLI 驗證座標關係。

**前置準備**：[Phase 11 Launch 進階](../phase-11-launch-files-advanced/) — 本章需要寫 launch file。

**產出目標**：[`code/my_robot_description/`](code/my_robot_description/) — 完整的兩輪差速車 URDF + display launch。

**環境**：☁️ TheConstructSim + 💻 本機 WSL 雙環境通用。

---

## 🌉 Part 3 → Part 4:從「邏輯機器人」到「物理機器人」

Part 1–3 你學完所有**通訊機制**(Pub/Sub/Service/Action/Lifecycle)。但你的 Capstone 1 ApproachController 是個**邏輯概念** — 不知道 lidar 裝在哪、輪距多寬、attack 點是什麼方向。

**Part 4 開始，Node 們終於擁有了一個具象化的身體**：

- **Phase 15 (URDF) — 賦予骨架與外型**：我們將用 XML 語法撰寫 URDF 檔案，精確定義出機器人的每一個關節 (joint) 和連桿 (link)，以及各種感測器在實體空間中的相對位置。
- **Phase 16 (TF2) — 賦予空間感知能力**：有了骨架後，我們需要 TF2 系統來處理錯綜複雜的座標轉換。讓機器人能瞬間算出「光達看到的障礙物，相對於車輪中心到底在哪裡」。
- **Phase 17 (Gazebo) — 賦予物理法則**：只有外殼是不夠的，我們要把這個 URDF 身體丟進 Gazebo 物理引擎中，為它加上重力、摩擦力與碰撞體積，讓它真的能夠撞牆、滾動與受力。
- **Phase 18 (ros2_control) — 賦予肌肉與神經**：讓純粹的軟體 Node 能夠透過標準化介面，將速度或扭矩指令轉化為驅動關節與馬達的真實動作。
- **Phase 19 (pluginlib) — 賦予大腦擴充性**：學會使用 Plugin 機制，讓你可以像裝載模組一樣，隨時替換機器人的控制器 (Controller) 或路徑規劃器 (Planner)。
- **Phase 20 (多機通訊) — 走入實機部署**：在真實的工業場景中，通常是一台運算力強大的電腦負責跑 AI 視覺感知，另一台即時性強的微控制器負責跑馬達控制。我們將學會如何讓它們跨裝置無縫通訊。

**為什麼 SLAM、Nav2、MoveIt 這些超酷的應用不能直接學？**：因為它們全部都假設你已經擁有了一份完美的 URDF 檔案。沒有實體身體就沒有 TF tree，沒有 TF tree 演算法就絕對沒辦法把雷射光達掃描到的障礙物，正確地標記在 2D 地圖上。

---

## 📋 開始之前:先修速查

這章假設你已經會幾個基礎,沒概念的話花 3 分鐘看完:

### XML 階層

```xml
<robot name="my_robot">           <!-- 最外層 -->
  <link name="base">              <!-- 一個元件 -->
    <visual>...</visual>          <!-- 子元件 -->
  </link>
  <joint name="left_wheel" ...>   <!-- 連接兩個 link 的關節 -->
    <parent link="base"/>
    <child link="wheel"/>
  </joint>
</robot>
```

**會 HTML 就會 XML**,差別只在 XML tag 自己定義 + 大小寫敏感。

### 3D 座標 + RPY 旋轉

URDF 用 `<origin xyz="x y z" rpy="roll pitch yaw"/>` 表示位置與姿態:

| 軸 | 對應方向 | 旋轉角 |
|---|---------|--------|
| **x** | 機器人**前方** | roll(沿 x 軸轉,「翻車」)|
| **y** | 機器人**左方** | pitch(沿 y 軸轉,「點頭」)|
| **z** | 機器人**上方** | yaw(沿 z 軸轉,「轉彎」)|

**單位**:長度 = 公尺,角度 = 弧度(`3.14` ≈ 180°)。

### URDF / xacro / SDF — 三個格式的關係(預告)

這章只用 URDF + xacro,但你之後會撞到 SDF。**先講清楚避免 Phase 17 卡住**:

| 格式 | 誰用 | 本章用嗎 |
|------|------|---------|
| **URDF** | RViz / robot_state_publisher / TF tree(本章主角) | ✅ |
| **xacro** | URDF 的「巨集系統」,用 `<macro>` 重用 link/joint(本章會用) | ✅ |
| **SDF** | Gazebo 物理引擎自己的格式(Phase 17 才出場) | ❌ |

**Gazebo 為什麼要另一套 SDF**:Gazebo 比 RViz 多了物理屬性(摩擦、阻尼、感測器 plugin),URDF 表達不了這些 → 通常用 URDF 寫,Gazebo 啟動時轉成 SDF。**Phase 17 會看到 turtlebot3 同時提供 URDF(給 robot_state_publisher)+ SDF(給 Gazebo 物理引擎),這是業界常態,不是 bug**。

---

## 🤔 為什麼要 URDF

到 Phase 14 為止，你所開發的「機器人」其實只是活在終端機裡的邏輯概念：`fake_lidar` 負責發送偽造的數字，`smart_brake` 負責接收並運算。但**真實世界的機器人是有具體形狀、且佔據物理空間的**：
- 你的光達是安裝在車頂正前方，還是偏向左側？
- 兩個驅動輪之間的輪距到底是 30 公分還是 50 公分？這直接影響了車子差速轉彎的半徑計算。
- 深度攝影機是水平擺放，還是為了看清地面障礙物而朝下傾斜了 15 度？

**URDF (Unified Robot Description Format)** 就是用來精確描述這些**三維幾何關係與物理屬性**的國際標準格式。千萬不要小看這個 XML 檔案，**舉凡 SLAM 建圖、Nav2 導航、MoveIt 機械臂夾取、Gazebo 物理模擬、到 RViz 的視覺化，全部都必須依賴 URDF 才能運作。沒有 URDF，你的軟體大腦就等於是又瞎又癱的**。

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

### 🤔 為什麼用 xacro 而不是純 URDF

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

- **URDF 描述實體結構**：掌握了利用 XML 標籤（如 `link` 定義實體部位、`joint` 定義連接關係、`origin` 定義相對位置）來精確建立機器人的 3D 結構。
- **Xacro 的巨集魔法**：了解純 URDF 的維護有多痛苦後，學會使用 Xacro 這個強大的前處理器，透過變數定義、巨集展開與模組化引入，寫出乾淨俐落的硬體描述檔。
- **靈活運用三大 Joint**：搞懂了 `fixed` (死鎖固定)、`continuous` (無限旋轉如車輪) 以及 `revolute` (有角度限制如機械臂) 的差異與應用場景。
- **系統的樞紐 (`robot_state_publisher`)**：它就像是個辛勤的翻譯官，負責把靜態的 URDF 結構檔，加上動態的 `/joint_states` 關節角度變化，即時計算並廣播成整棵空間座標樹 (`/tf`)。
- **TF 除錯雙雄**：熟練使用 `tf2_echo` 在終端機即時偷看兩個部位的座標關係，以及用 `view_frames` 產出 PDF 架構圖，這在未來 Debug 空間迷失問題時非常關鍵。
- **開源生態系的入場券**：再次強調，不論是玩 Nav2 導航還是 MoveIt 夾取，第一步永遠是寫好 URDF。你已經跨過了實機開發最重要的一道硬核門檻。

---

## 🌟 進階挑戰

1. **加 base_footprint**：在 base_link 下方加一個 dummy link 當真正 root，消掉 KDL 警告
2. **xacro `<xacro:include>`**：把 wheel macro 拆到獨立 xacro 檔
3. **多機器人**：用 xacro 接受 `<xacro:arg name="robot_name">`，產生 robot1/robot2 兩台不同 namespace 的車
4. **加機械臂**：在 base 上加一個簡單的兩節 arm（base → arm_base → arm_link1 → arm_tip）

---

## 👣 下一步去哪？

- [Phase 16 — TF2](../phase-16-tf2/)：學會用 C++/CLI 操作 TF tree、處理時間戳、寫 broadcaster/listener

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
