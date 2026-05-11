# Phase 20B:手臂 URDF (xacro + SRDF)

> 這章從「能描述一個 robot」進到「能描述一支可規劃的手臂」。我們會用 xacro macro 寫出 6-DOF 機械手臂,再補上 SRDF,讓下一章 MoveIt 可以直接接手。

**學完你會**:
- 用 xacro `<xacro:macro>` 把重複的 link+joint pattern 抽成參數化模板
- 用 `xacro:include` 拆檔,讓主檔保留設計意圖,macro 檔負責重複結構
- 寫 SRDF(`group`、`group_state`、`disable_collisions`),讓 MoveIt 知道哪一段 chain 是手臂
- 在 launch 內展開 xacro,並避開「`robot_description` 被當 YAML 解析」這個經典雷
- 用 `check_urdf` 跟 `tf2_echo` 純 CLI 驗證 kinematic chain 是對的

**前置**:
- [Phase 15 URDF](../phase-15-urdf/) — URDF 基礎(link、joint、TF tree)。本章是 URDF 進階版,專攻多 DOF
- [Phase 16 TF2](../phase-16-tf2/) — 驗證 TF tree 用

**產出**:
- [`urdf/arm.macro.xacro`](code/my_arm_description/urdf/arm.macro.xacro) — 抽出的 link+joint macro
- [`urdf/arm.urdf.xacro`](code/my_arm_description/urdf/arm.urdf.xacro) — 主檔,呼叫 macro 6 次組出整支 arm
- [`srdf/arm.srdf`](code/my_arm_description/srdf/arm.srdf) — MoveIt 用的語意描述
- [`launch/display.launch.py`](code/my_arm_description/launch/display.launch.py) — robot_state_publisher + joint_state_publisher

**環境**:☁️💻 雙環境通用(CLI 驗證 + 可選 RViz GUI)

---

## 為什麼這章重要

Phase 15 的 URDF 可以描述一台小車,但機械手臂的描述會很快變大。業界常見手臂 URDF 有三個特徵:

1. **長**:UR5、Franka、Aubo 這類官方描述檔常常是 1000–3000 行
2. **重複**:6 軸手臂的 link/joint pattern 很像,手寫會一直複製貼上
3. **多版本**:不同型號、左右手、不同 payload、不同長度都會衍生新版本

不用 xacro 的話,改一個 link 長度可能要同步改 visual、collision、inertia 好幾個地方。用 macro 之後,你改參數,整支手臂會一起更新。

SRDF 則是 MoveIt 需要的「語意描述」。URDF 只知道有哪些 link/joint;SRDF 會補上「哪段 chain 是 arm」、「有哪些 named pose」、「哪些 link 不需要檢查碰撞」。Track B 後面的 Phase 21B 會直接吃這些資訊。

---

## 🏗️ 手臂設計

```
TF 樹(8 個 link、6 DOF + 2 fixed):

  world ──fixed── base_link
                     │ j1: shoulder pan  (z)  ±π
                     ▼
                   link_1
                     │ j2: shoulder lift (y)  ±π/2
                     ▼
                   link_2
                     │ j3: elbow         (y)  ±2.5
                     ▼
                   link_3
                     │ j4: wrist roll    (z)  ±π
                     ▼
                   link_4
                     │ j5: wrist pitch   (y)  ±π/2
                     ▼
                   link_5
                     │ j6: tool roll     (z)  ±π
                     ▼
                   link_6
                     │ fixed
                     ▼
                   tool0  (工具 mounting point)
```

這支手臂採用常見的 6R serial arm 配置。前 3 軸主要決定末端位置,後 3 軸像 spherical wrist,主要調整末端姿態。這樣安排能在簡化模型的同時保留「像真手臂」的運動特性。

---

## 🕵️ 終端機偵探課:不用 RViz 也能驗 URDF

RViz 很適合看模型,但第一輪 debug 不該只靠眼睛。手臂模型先用 CLI 檢查三件事:xacro 能不能展開、URDF chain 是否完整、TF 是否真的從 `base_link` 接到 `tool0`。

### 偵探 1:xacro 能不能展開成 URDF

先完成後面 Step 1 的部署與 build。若你開了新的 terminal,先 source ROS 環境:

```bash
source /opt/ros/humble/setup.bash
source ~/ros2_ws/install/setup.bash

xacro $(ros2 pkg prefix my_arm_description)/share/my_arm_description/urdf/arm.urdf.xacro \
  > /tmp/arm.urdf
wc -l /tmp/arm.urdf
```

預期看到:

```text
249 /tmp/arm.urdf
```

如果這一步失敗,通常是 `xacro:include` 路徑錯、套件沒 build,或 launch 裡找不到 package share directory。

### 偵探 2:kinematic chain 是否完整

```bash
check_urdf /tmp/arm.urdf
```

預期看到 `Successfully Parsed XML`,而且 chain 從 `world` 一路接到 `tool0`:

```text
root Link: world
  base_link
    link_1
      link_2
        ...
          tool0
```

如果有孤立 link、joint parent/child 寫錯,這裡會比 RViz 更早告訴你。

### 偵探 3:TF 是否真的發出來

Terminal 1 啟動 launch:

```bash
ros2 launch my_arm_description display.launch.py
```

Terminal 2 查 topic 和 TF:

```bash
ros2 topic list | grep -E "joint_states|robot_description|tf"
ros2 run tf2_ros tf2_echo base_link tool0
```

預期看到 `/joint_states`、`/robot_description`、`/tf`、`/tf_static`,以及 `base_link → tool0` 的 transform。

**這章要做的事**:先用 terminal 確認模型資料是健康的,再開 RViz 做視覺驗證。這樣看到空白畫面時,你會知道該查 Fixed Frame,而不是懷疑 URDF 全壞了。

---

## 💻 重點檔案

### 1. arm.macro.xacro — 一段 link + joint 的模板

完整見 [`urdf/arm.macro.xacro`](code/my_arm_description/urdf/arm.macro.xacro)。

```xml
<xacro:macro name="arm_link"
             params="name parent length radius axis origin_xyz lower upper">

  <joint name="${name}_joint" type="revolute">
    <parent link="${parent}"/>
    <child link="${name}"/>
    <origin xyz="${origin_xyz}" rpy="0 0 0"/>
    <axis xyz="${axis}"/>
    <limit lower="${lower}" upper="${upper}" effort="100" velocity="1.5"/>
  </joint>

  <link name="${name}">
    <visual>...</visual>
    <collision>...</collision>
    <inertial>
      <mass value="1.0"/>
      <inertia
        ixx="${(1/12) * 1.0 * (3*radius*radius + length*length)}"
        iyy="${(1/12) * 1.0 * (3*radius*radius + length*length)}"
        izz="${(1/2)  * 1.0 * radius*radius}"/>
    </inertial>
  </link>

</xacro:macro>
```

**亮點**:`${...}` 裡可以寫運算,所以 `inertia` 可以直接套用 solid cylinder 的公式。之後只要改半徑或長度,慣量會自動跟著更新,不用手動重算 6 次。

### 2. arm.urdf.xacro — 主檔呼叫 macro 6 次

```xml
<xacro:include filename="$(find my_arm_description)/urdf/arm.macro.xacro"/>

<link name="world"/>
<joint name="world_to_base" type="fixed">
  <parent link="world"/>
  <child link="base_link"/>
</joint>
<link name="base_link">...</link>

<xacro:arm_link name="link_1" parent="base_link"
                length="0.20" radius="0.05"
                axis="0 0 1" origin_xyz="0 0 0.10"
                lower="-3.14" upper="3.14"/>

<xacro:arm_link name="link_2" parent="link_1"
                length="0.30" radius="0.04"
                axis="0 1 0" origin_xyz="0 0 0.20"
                lower="-1.57" upper="1.57"/>
<!-- ... 重複 4 次,每次只改參數 -->
```

主檔維持在約 80 行,展開後的純 URDF 是 **249 行**(實測)。也就是說,xacro 讓你讀的是設計,不是展開後的重複 XML。

### 3. arm.srdf — 為 MoveIt 鋪路

完整見 [`srdf/arm.srdf`](code/my_arm_description/srdf/arm.srdf)。

```xml
<group name="arm">
  <chain base_link="base_link" tip_link="tool0"/>
</group>

<group_state name="home" group="arm">
  <joint name="link_1_joint" value="0"/>
  ...
</group_state>

<group_state name="ready" group="arm">
  <joint name="link_2_joint" value="-1.0"/>
  <joint name="link_3_joint" value="1.0"/>
  ...
</group_state>

<disable_collisions link1="link_1" link2="link_2" reason="Adjacent"/>
```

MoveIt 拿到 SRDF 後,planner 才知道 `base_link → tool0` 這段 chain 叫做 `arm`,也知道 `home` / `ready` 這些 named pose,以及哪些相鄰 link 不需要檢查碰撞。到 Phase 21B 啟動 MoveIt Setup Assistant 時,這份 SRDF 就能直接作為基礎。

### 4. display.launch.py — 在 launch 內展開 xacro

```python
from launch.substitutions import Command
from launch_ros.parameter_descriptions import ParameterValue

robot_description = ParameterValue(
    Command(['xacro ', xacro_file]),
    value_type=str           # ⚠️ 必須這個,不然 launch 會把 URDF 當 YAML 解析失敗
)

return LaunchDescription([
    Node(package='robot_state_publisher',
         executable='robot_state_publisher',
         parameters=[{'robot_description': robot_description}]),
    Node(package='joint_state_publisher',
         executable='joint_state_publisher'),
])
```

`Command(['xacro ', file])` 會在 launch runtime 執行 `xacro`,把展開後的 URDF string 餵給 `robot_state_publisher`。如果沒有用 `ParameterValue(..., value_type=str)` 包住,launch 會把這段 XML 當 YAML parse,看到 `<robot>` 就失敗。

---

## 🚀 完整 Demo 流程(WSL,驗證過)

### Step 1:部署 + build

```bash
rm -rf ~/ros2_ws/src/my_arm_description
cp -r /mnt/d/ros_learn/ros2-learning-notes/phase-20B-arm-urdf/code/my_arm_description \
      ~/ros2_ws/src/my_arm_description
source /opt/ros/humble/setup.bash
cd ~/ros2_ws && colcon build --packages-select my_arm_description
```

這個 package 名稱本來就是 `my_arm_description`,不用像某些教學 demo 那樣另外改名。

### Step 2:展開 xacro 看大小

```bash
source ~/ros2_ws/install/setup.bash
xacro $(ros2 pkg prefix my_arm_description)/share/my_arm_description/urdf/arm.urdf.xacro \
      > /tmp/arm.urdf
wc -l /tmp/arm.urdf
```

驗證過輸出:
```
249 /tmp/arm.urdf
```

xacro 主檔約 80 行,展開成純 URDF 後是 249 行。重點不是單純省行數,而是主檔讀起來保留了手臂的設計結構。

### Step 3:check_urdf 驗證 chain

```bash
$ check_urdf /tmp/arm.urdf
robot name is: my_arm
---------- Successfully Parsed XML ---------------
root Link: world has 1 child(ren)
    child(1):  base_link
        child(1):  link_1
            child(1):  link_2
                child(1):  link_3
                    child(1):  link_4
                        child(1):  link_5
                            child(1):  link_6
                                child(1):  tool0
```

這裡要確認的是:8-link kinematic chain 完整、沒有孤立 link、沒有 joint 迴圈。`check_urdf` 通過代表 URDF 的基本結構是健康的。

### Step 4:啟動 launch + 驗 TF

```bash
ros2 launch my_arm_description display.launch.py &
sleep 5
ros2 topic list
ros2 run tf2_ros tf2_echo base_link tool0
```

驗證過輸出:
```
=== TOPICS ===
/joint_states
/parameter_events
/robot_description
/rosout
/tf
/tf_static

=== TF base_link -> tool0 ===
- Translation: [0.000, 0.000, 1.100]
- Rotation: in Quaternion (xyzw) [0.000, 0.000, 0.000, 1.000]
```

`Translation [0,0,1.10]` 對應 base_link 之上 link_1 到 tool0 的高度疊加:0.20+0.30+0.25+0.10+0.10+0.05+0.05 = 1.05,再加 base 0.05 = 1.10。這表示 TF chain 的幾何關係是對的。

### Step 5:RViz 視覺驗證(WSL 驗證過,有截圖)

#### Step 5a:啟動 launch 帶 GUI slider

```bash
ros2 launch my_arm_description display.launch.py gui:=true
# (gui:=true → 啟動 joint_state_publisher_gui,可拉 6 個 joint slider 即時轉動手臂)
# (預設或 gui:=false → joint 全 0,手臂直立)
```

#### Step 5b:另開 terminal 啟動 RViz

```bash
rviz2
```

#### Step 5c:RViz 設定(2 步驟)

**1. Fixed Frame** 改成 `world`。RViz 預設是 `map`,但這支手臂沒有 `map` frame,詳見雷 7。
**2. Add → RobotModel**。展開 RobotModel 後,設 `Description Source: Topic` + `Description Topic: /robot_description`。

⚠️ **如果你忘了改 Fixed Frame**,看到的是這樣(空 grid + 紅色 Error):

![RViz 雷:Fixed Frame=map 找不到 frame](images/rviz-empty-fixed-frame-error.png)

設定完成後,你會看到底座 + 直立手臂。這是 home pose,也就是所有 joint 都為 0 的狀態:

![RViz 顯示手臂 home pose](images/rviz-arm-loaded-home-pose.png)

#### Step 5d:互動 — 拉 slider 即時轉手臂

`gui:=true` 啟動的 `Joint State Publisher GUI` 視窗有 6 個 slider(`link_1_joint` ~ `link_6_joint`)+ `Randomize` / `Center` 兩個按鈕。

拉動 slider 後,RViz 裡的對應關節會即時轉動:

![RViz + Joint State Publisher GUI 互動拉 slider](images/rviz-arm-with-jsp-gui.png)

實測畫面:
- `link_1_joint=1.816`(肩膀繞 z 軸轉接近 90°)
- `link_2_joint=0.636`(肩膀往前彎)
- `link_3_joint=0.933`(手肘彎)
- `link_4_joint=1.476`(手腕 roll)
- `link_5_joint=-1.095`(手腕 pitch 往下)
- 整支手臂彎成 L 形,tool0 朝外指向

按 **`Randomize`** 可以把 6 個 joint 隨機設定一次,快速檢查模型是否能正常動。
按 **`Center`** 會把所有 joint 歸 0,回到直立 home pose。

---

## 🐛 常見雷

### ⚠️ 雷 1:`Unable to parse the value of parameter robot_description as yaml`

**症狀**:`ros2 launch` 馬上炸:
```
Caught exception in launch: Unable to parse the value of parameter robot_description
as yaml. If the parameter is meant to be a string, try wrapping it in
launch_ros.parameter_descriptions.ParameterValue(value, value_type=str)
```

**原因**:`Command(['xacro', file])` 輸出是 string,但 launch 預設把 parameter 值當 YAML parse,看到 URDF 第一行 `<robot ...>` 就失敗。

**解**:用 `ParameterValue(..., value_type=str)` 明確標型別:
```python
from launch_ros.parameter_descriptions import ParameterValue
robot_description = ParameterValue(
    Command(['xacro ', xacro_file]),
    value_type=str)
```

這個錯誤訊息其實已經提示了解法,但許多舊範例仍然直接把 `Command(...)` 當 parameter 傳入,沒有包 `ParameterValue`。如果你照抄舊寫法,就會遇到這個問題。

### ⚠️ 雷 2:xacro 沒 macro 6 個 link 各自手寫,改尺寸 6 次

**症狀**:URDF 一開始寫得很快,但後來要把 `link_3` 加長 1cm,你才發現 visual、collision、inertia 都要一起改。

**解**:一開始就用 macro。長度、半徑、joint axis 都變成參數,慣量公式也放進 macro:
```xml
ixx="${(1/12) * 1.0 * (3*radius*radius + length*length)}"
```

### ⚠️ 雷 3:visual / collision / inertial 的 `<origin>` 容易忘

**症狀**:RViz 看手臂 link 都「半截」插在 parent 裡。

**原因**:cylinder 的幾何中心在原點,但我們希望 link 的底端落在 joint 位置。因此 visual / collision / inertial 都要有對應的 `<origin xyz="0 0 length/2">`。

**解**:macro 內統一處理,不要每個 link 自己寫。

### ⚠️ 雷 4:慣量寫零 / 隨便填,Gazebo 不會抱怨,但機器人在裡面亂飛

**症狀**:Gazebo 啟動後手臂自己彈飛、震盪、卡 joint。

**原因**:`<inertial>` 的 ixx/iyy/izz 是物理模擬必需的物理量。寫成 0 → 質量無限大或無限小,模擬失穩。

**解**:使用標準幾何體的解析公式:
- Cylinder: `ixx = iyy = (1/12) m (3 r² + h²)`,`izz = (1/2) m r²`
- Box: `ixx = (1/12) m (h² + d²)` 等
- 不確定時可以用 mesh 慣量產生器(`compute_inertia_from_mesh`),或先用 mass=1、ixx=iyy=izz=0.01 當佔位符。不要寫 0。

### ⚠️ 雷 5:`disable_collisions` 漏寫,MoveIt 規劃變超慢

**症狀**:MoveIt 規劃個簡單動作要 5 秒以上。

**原因**:沒有 `disable_collisions` 時,MoveIt 在每個規劃 step 都會檢查 N(N-1)/2 對 link 的碰撞。8 個 link 就是 28 對。相鄰 link 永遠相連,通常不需要每一步都做碰撞檢查。

**解**:**用 MoveIt Setup Assistant 一次跑 sample-based 分析,自動生成 `disable_collisions` 列表**。手寫只寫 adjacent 的,Setup Assistant 會多找出「永遠不可能撞到」的非相鄰 pair(例如 link_1 跟 link_5 在你的關節限制下永遠不會接觸)。

### ⚠️ 雷 6:`joint_state_publisher` 跟 `joint_state_publisher_gui` 雙發 → 手臂在 RViz 內跳動

**症狀(實測)**:RViz 內手臂在「slider 設定的角度」和「全 0 home pose」之間反覆切換。

**原因**:**兩個 source 在搶同一個 `/joint_states` topic** —
- `joint_state_publisher`(launch 預設啟,持續發全 0)
- `joint_state_publisher_gui`(你後來開的,發拉動的值)

兩者輪流 publish,RViz subscriber 收到哪一筆就顯示哪一筆,所以畫面會跳。

**解**:本章 launch 用 `gui` arg 互斥兩者:
```bash
ros2 launch my_arm_description display.launch.py gui:=true   # 只起 GUI 版
ros2 launch my_arm_description display.launch.py             # 預設只起 CLI 版
```

如果你已經誤同時跑了,直接 `pkill -9 -f 'joint_state_publisher$'`(`$` 結尾只殺 non-GUI 版)。

### ⚠️ 雷 7:RViz 開啟後 Fixed Frame 預設是 `map` → 紅色 Error,看不到任何東西

**症狀(實測)**:打開 RViz 看到空 grid,左上 Displays 區塊有紅 ❌ `Fixed Frame: Frame [map] does not exist`。

**原因**:RViz 預設 `Fixed Frame` 是 `map`,但這支手臂沒有 SLAM/Nav2 → 沒有 `/map` frame。本章手臂的 root frame 是 `world`(URDF 內定義)。

**解**:Displays → Global Options → Fixed Frame → 點下拉選 **`world`**(或直接打字)。紅錯誤秒消失。

> 這個雷對第一次玩 URDF 的人特別常見,因為很多 ROS 教學都是從 SLAM/Nav2 進 RViz,大家很容易習慣 Fixed Frame=`map`。

### ⚠️ 雷 8:`world` link 的存廢

**症狀**:看別人的 URDF 有的有 `world` link 有的沒有。

**規則**:
- **手臂** 通常需要 `world` link + `world→base_link` fixed joint。MoveIt scene、planning frame 常用 `world` 當原點
- **移動底盤** 通常不要 `world` link,而是用 `odom→base_footprint→base_link` 結構,讓 SLAM/Nav2 動態維護 odom→base_footprint
- 兩個一起裝(mobile manipulator)用 `world→map→odom→base_footprint→base_link→arm_base_link`

---

## 🎯 學到的關鍵概念

| 概念 | 一句話 |
|------|------|
| `<xacro:macro>` | 把重複 pattern 抽成模板,改一處全動 |
| `<xacro:include>` | 拆 macro 跟主檔,大型 URDF 必用 |
| `${expression}` | xacro 內可寫運算,慣量、長度算式直接內嵌 |
| `<robot>` 內 SRDF | MoveIt 拿 SRDF 知道 chain / named pose / 不撞 link |
| `Command(['xacro', f])` | launch runtime 展開 xacro,但要 `ParameterValue(..., str)` |
| 軸交替 z/y/y/z/y/z | 6R 手臂典型設計,保證 dexterous workspace |

---

## 🌟 進階挑戰

1. **加 gripper**:寫一個 `gripper.macro.xacro`(兩根對稱 prismatic finger),`xacro:include` 進主檔,`group_state` 加 `open` / `closed`
2. **加 IMU/camera link**:在 link_6 上掛感測器 link,fixed joint,寫真實 offset
3. **xacro 條件**:用 `<xacro:if value="${has_gripper}">` 讓主檔可選擇是否帶夾爪
4. **mesh 視覺**:把圓柱換成 OnShape / SolidWorks 匯出的 STL/DAE,實機照片級的視覺
5. **Gazebo plugin**:加 `<gazebo:plugin>` 讓 ros2_control 能驅動,接 Phase 18 的 controller_manager

---

## 🔗 下一步

- **[Phase 21B MoveIt Setup Assistant](../phase-21B-moveit-setup-assistant/)** ⏸ — 把本章 URDF + SRDF 餵給 GUI wizard 自動產 MoveIt config(草稿,截圖待補)
- **[Phase 22B MoveIt C++](../phase-22B-moveit-cpp/)** — 用 `MoveGroupInterface` 跑軌跡規劃(可手寫 yaml,跳過 21B 也能進)
- **[Phase 18 ros2_control](../phase-18-ros2-control/)** — 回頭看「URDF 怎麼宣告 hardware」,把這支手臂變成可控制的
- **[Phase 15 URDF 基礎](../phase-15-urdf/)** — 對照看單關節 mobile robot 與多 DOF arm 的 URDF 差異

---

## 📁 完整檔案結構

```
phase-20B-arm-urdf/
├── README.md
├── code/
│   └── my_arm_description/
│       ├── package.xml
│       ├── CMakeLists.txt
│       ├── urdf/
│       │   ├── arm.urdf.xacro            ← 主檔,呼叫 macro 6 次
│       │   └── arm.macro.xacro           ← link+joint macro
│       ├── srdf/
│       │   └── arm.srdf                  ← MoveIt 用,group / named pose / disable_collisions
│       ├── launch/
│       │   └── display.launch.py         ← robot_state_publisher + joint_state_publisher
│       └── rviz/                         ← (之後補:rviz config)
└── images/
    ├── rviz-empty-fixed-frame-error.png  ← RViz Fixed Frame 錯誤示例
    ├── rviz-arm-loaded-home-pose.png     ← 手臂 home pose
    └── rviz-arm-with-jsp-gui.png         ← GUI slider 互動畫面
```
