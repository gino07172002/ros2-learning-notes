# Phase 21B:MoveIt 2 Setup Assistant — GUI 自動產 config

> Phase 20B 的 6-DOF 手臂 URDF + SRDF 寫好了,但 Phase 22B 要餵給 `MoveGroupInterface` 的**4 個 yaml**(kinematics / OMPL / joint_limits / controllers)還沒生出來。Setup Assistant 是 MoveIt 官方的 GUI wizard,**幾分鐘自動產出整套 moveit_config 套件**,業界 100% 用它,不會手寫。

**這章你將解鎖的業界 MoveIt 技能**：
- **駕馭官方外掛神器 (`moveit_setup_assistant`)**：不再盯著空白的文字檔發呆。你將學會如何透過圖形化介面，將 Phase 20B 寫好的 URDF 骨架匯入，讓系統自動幫你剖析機器人的關節極限與連桿關係。
- **配置四大核心模組**：透過點擊與選單，輕鬆設定最複雜的「自我碰撞矩陣 (Self-Collision Matrix)」、「規劃群組 (Planning Groups)」、「硬體控制器 (Controllers)」以及「運動學求解器 (Kinematics Solver)」。
- **一鍵生成企業級設定檔**：見證奇蹟的時刻。讓 Setup Assistant 幫你自動產生一整個 `<arm>_moveit_config` 套件。這裡面包含了成千上萬行的 YAML 與 Launch 檔，完全取代我們在教學階段辛苦手刻的配置文件。
- **碰撞矩陣的底層邏輯**：深刻理解為什麼在 SRDF 中大量標記 `disable_collisions` 其實是一種「為了加速運算的安全妥協」，而不是演算法的漏洞。這能讓你的路徑規劃速度提升好幾個數量級。

**前置**:
- [Phase 20B 手臂 URDF](../phase-20B-arm-urdf/) — 提供 xacro/URDF + 已寫好的 SRDF(會被 Setup Assistant 覆蓋)
- 本章**只能在本機 WSL 跑**(需要 GUI)

**產出**:
- `code/my_arm_moveit_config/` — Setup Assistant 自動生成(本 repo 目前尚未放入,等本機 GUI 跑完後補),含:
  - `config/kinematics.yaml`、`config/ompl_planning.yaml`、`config/joint_limits.yaml`、`config/moveit_controllers.yaml`
  - `config/<arm>.srdf`(更新版,含 self-collision matrix)
  - `launch/`(demo / move_group / setup_assistant 多支)

**環境**:💻 **本機 WSL2 only**(Setup Assistant 是純 GUI 工具,雲端 ROSject 限制多)

---

## 🌉 為什麼有這章

**為什麼我們需要學這套 GUI 工具？**

- **從「手刻學習」到「自動量產」**：在教學範例中，我們會展示最基礎的 YAML 檔，那是為了讓你弄懂每一行參數背後的物理意義。但在真實工業界，沒有人會手寫這幾千行的配置檔，全部都是透過 Setup Assistant GUI 一鍵生成的。
- **算力極限的突破 (自我碰撞矩陣)**：如果用手寫，你最多只能設定「相鄰的兩個連桿不計算碰撞」。但 Setup Assistant 會在背景啟動物理引擎，對你的機器人進行高達 10,000 次的隨機姿態取樣，精準抓出「這輩子絕對不可能碰在一起的連桿組合」並將其碰撞檢測關閉。這一招能讓你的路徑規劃速度快上好幾倍。
- **無痛抽換演算法**：想要測試 RRTConnect 與 PRM 演算法的優劣？想要把位置控制器換成軌跡控制器？不再需要翻找深藏在資料夾底層的 YAML 檔，GUI 上點兩下就能完成設定檔的覆寫。
- **拖曳式的架構設計**：要新增一個夾爪 (Gripper) 群組，或是改變機器手群組包含的關節，都可以透過視覺化的選單直接拖曳完成，徹底告別容易寫錯 tag 的 XML 噩夢。

**學習心法**：本章教你**「業界如何快速產出骨架」**，而其他章節教你**「骨架裡面裝了什麼藥」**。在真實專案中，永遠是先用這套 GUI 產出基礎包，再憑著你的底層知識進去微調參數。

---

## 🛠️ 步驟 1:啟動 Setup Assistant

```bash
# 安裝(若還沒裝)
sudo apt install ros-humble-moveit-setup-assistant

# 啟動 GUI
ros2 run moveit_setup_assistant moveit_setup_assistant
```

預期跳出標題列為「**MoveIt Setup Assistant**」的視窗。

> 📷 截圖位:`images/setup-assistant-welcome.png` _(待補:GUI 首頁,有「Create New」「Edit Existing」兩個大按鈕)_

---

## 🛠️ 步驟 2:載入 Phase 20B 的 URDF

點 **Create New MoveIt Configuration Package**,然後 **Browse** 選:

```
d:/ros_learn/ros2-learning-notes/phase-20B-arm-urdf/code/my_arm_description/urdf/<arm>.urdf.xacro
```

(WSL 內路徑:`/mnt/d/ros_learn/...`)

點 **Load Files** 後右側會 render 出機械手臂模型。

> 📷 `images/setup-assistant-load-urdf.png` _(待補:左側 file panel,右側 3D viewer 顯示 6-DOF arm)_

**雷**:URDF 內若有 `package://` reference 但對應 package 沒在 workspace 內,會 load 失敗。確認 `my_arm_description` 已 colcon build 過 + source 過。

---

## 🛠️ 步驟 3:Self-Collision Matrix(必做)

左側 sidebar 第一項 **Self-Collisions**。

- **Sampling Density**:預設 10000(就用預設,不要動)
- 點 **Generate Collision Matrix**

跑幾秒後會列出「永遠不碰 / 預設不碰 / 相鄰 link / 永遠碰」四類 link pair,**勾選 Disable** 那些被判定不會碰的對。

**為什麼這步重要**:MoveIt 規劃時每個 trajectory point 都做 collision check,**每禁用一個 pair = 規劃快幾 ms**。一般手臂可禁用 60–80% 的 pair。

> 📷 `images/setup-assistant-self-collisions.png` _(待補:跑完 Generate 後的 Disabled Link Pairs 表格)_

> ⚠️ **不要手動關掉「Adjacent」(相鄰 link)以外的 pair** — 演算法已經算過,人工亂關會在規劃時撞到。

---

## 🛠️ 步驟 4:Virtual Joints(可選)

把手臂的 base link 接到「世界」。給移動底盤上的手臂時必設,**固定平台手臂可省略**。

範例(固定在 `world` frame):

| 欄位 | 值 |
|------|-----|
| Virtual Joint Name | `virtual_joint` |
| Child Link | `base_link`(arm 的 root link) |
| Parent Frame | `world` |
| Joint Type | `fixed` |

---

## 🛠️ 步驟 5:Planning Groups(核心步驟)

**最重要的一步**。手臂要被規劃必須先定義 group。

點 **Add Group**:

| 欄位 | 值 |
|------|-----|
| Group Name | `arm` |
| Kinematic Solver | `kdl_kinematics_plugin/KDLKinematicsPlugin`(預設,夠用) |
| Group Default Planner | `RRTConnect` |

然後選**怎麼定義 group**(三選一):

- **Add Joints**:逐一勾 6 個 joint(`joint_1` ~ `joint_6`)— 推薦
- **Add Links**:勾 link 也行
- **Add Kin. Chain**:選 base + tip 自動推導 — 最省事但有時抓錯

> 📷 `images/setup-assistant-planning-groups.png` _(待補:Add Group 對話框 + Joints 勾選列表)_

**夾爪要另外建 group**(如果你的手臂有夾爪):
- Group Name: `gripper`
- 勾選夾爪相關 joint
- Kinematic Solver: `None`(夾爪不需要 IK)

---

## 🛠️ 步驟 6:Robot Poses(預設姿態)

定義常用姿態,Phase 22B 用 `setNamedTarget("home")` 就會用這個。

建議至少建兩個:

| Pose Name | Group | 各 joint 值(弧度) |
|-----------|-------|-------------------|
| `home` | `arm` | 全 0 |
| `ready` | `arm` | 自己擺一個適合工作的姿態 |

點 **Save** 後可在右側 3D viewer 看到手臂 preview。

> 📷 `images/setup-assistant-robot-poses.png` _(待補:Robot Poses panel + 3D preview)_

---

## 🛠️ 步驟 7:End Effectors / Passive Joints(可選)

- **End Effectors**:有夾爪才設,告訴 MoveIt「規劃 arm 時把 gripper 視為末端」
- **Passive Joints**:被動關節(被其他關節帶動,例:平行連桿)— Phase 20B 沒這個,跳過

---

## 🛠️ 步驟 8:ros2_control(整合 controllers)

**Auto Add &lt;ros2_control&gt; Tag** 自動產生 ros2_control 必要 tag。

之後 **Auto Add JointTrajectoryController** — 自動配一個 controller 給 `arm` group。

> ⚠️ Phase 20B 的 URDF 若已有 `<ros2_control>` 區段,Setup Assistant 會問要不要覆蓋。**選 Yes**(讓 Setup Assistant 接管)。

---

## 🛠️ 步驟 9:MoveIt Controllers

把上一步的 ros2_control controller 連到 MoveIt 規劃介面。

點 **Auto Add Controllers** — 自動掃描 ros2_control yaml + 產出 `moveit_controllers.yaml`。

預期看到一個 entry:
```yaml
arm_controller:
  type: FollowJointTrajectory
  joints:
    - joint_1
    ...
    - joint_6
```

---

## 🛠️ 步驟 10:Author Information

填一下 Author Name + Email,會寫進產出 package 的 `package.xml`。**亂填也可以**,但**不要留空**(會 generate 失敗)。

---

## 🛠️ 步驟 11:Configuration Files(產出!)

**Browse** 選輸出資料夾:

```
d:/ros_learn/ros2-learning-notes/phase-21B-moveit-setup-assistant/code/my_arm_moveit_config
```

點 **Generate Package**,等幾秒。

預期產出:

```
my_arm_moveit_config/
├── config/
│   ├── joint_limits.yaml             # joint 速度/加速度上限
│   ├── kinematics.yaml               # IK solver 設定
│   ├── moveit_controllers.yaml       # MoveIt → ros2_control 對應
│   ├── ompl_planning.yaml            # OMPL planner 設定
│   ├── pilz_industrial_motion_planner_planning.yaml
│   ├── ros2_controllers.yaml         # ros2_control controller 定義
│   ├── <arm>.srdf                    # planning groups + collision matrix
│   └── ...
├── launch/
│   ├── demo.launch.py                # 一鍵啟動 RViz + move_group
│   ├── move_group.launch.py
│   ├── setup_assistant.launch.py     # 重新打開本 wizard 編輯
│   └── ...
├── package.xml
└── CMakeLists.txt
```

> 📷 `images/setup-assistant-generate-success.png` _(待補:Generate Package 成功的對話框)_

---

## 🚀 步驟 12:跑一下 demo 驗證

```bash
# 部署到 ros2_ws + build
# 等本機 GUI 產出 code/my_arm_moveit_config 後再複製
cp -r d:/ros_learn/ros2-learning-notes/phase-21B-moveit-setup-assistant/code/my_arm_moveit_config ~/ros2_ws/src/
cd ~/ros2_ws
colcon build --packages-select my_arm_moveit_config
source install/setup.bash

# 跑 demo
ros2 launch my_arm_moveit_config demo.launch.py
```

**成功指標**:RViz 跳出來、左側 MotionPlanning panel 載入完成、右側 3D 顯示手臂。
拖移 interactive marker 到一個目標位姿 → 點 **Plan** → 看到綠色軌跡 → 點 **Execute** → 手臂動到目標。

> 📷 `images/moveit-demo-rviz.png` _(待補:RViz 顯示 MotionPlanning panel + 規劃出的綠色軌跡)_

---

## 🎯 學到的關鍵概念

- **不是偷懶，是追求極致效能**：再次強調，使用 Setup Assistant 絕非工程師偷懶。它背後那套跑了 10,000 次蒙地卡羅取樣 (Monte Carlo Sampling) 的自我碰撞矩陣演算法，是人類手刻永遠無法企及的精準度。
- **結構分明的套件依賴**：你現在應該很清楚 `my_arm_description` (負責提供純粹的 URDF 外觀與物理屬性) 與 `my_arm_moveit_config` (負責提供 MoveIt 規劃演算法所需的各種 YAML 與 SRDF) 是兩個完全獨立卻又緊密相依的 ROS 2 Package。這是業界設計機器人軟體的黃金標準。
- **支援疊代開發 (`setup_assistant.launch.py`)**：當你在實機測試發現夾爪張開的角度設錯了，不需要從零開始跑精靈。只要執行套件內建的這支 Launch 檔，就能直接讀取舊配置並接續編輯。
- **知其然，也知其所以然**：當你學完這章，並把產出的複雜套件與後續章節手刻的「最小可行性 YAML」互相對照時，你對 MoveIt 底層架構的理解將會迎來一次量子級的躍升。

---

## ⚠️ 常見雷

### 雷 1:URDF load 失敗 — `package:// not found`

**症狀**:Step 2 點 Load Files 報「`Cannot find package:// reference`」。

**原因**:URDF 內 `<mesh filename="package://my_arm_description/meshes/..."/>`,但 my_arm_description 沒在當前 ROS 2 環境內。

**解**:先 `colcon build --packages-select my_arm_description && source install/setup.bash`,再啟動 Setup Assistant。

### 雷 2:Generate Package 卡在「validation failed」

**症狀**:Step 11 點 Generate 後跳 error,常常是「missing planning group」或「robot pose has invalid joint」。

**解**:回到 Step 5 / Step 6 看哪個 group / pose 還沒設完。**Author Info 留空也會失敗** — Step 10 別跳過。

### 雷 3:demo.launch.py 跑起來 RViz 顯示不出來手臂

**症狀**:RViz 開了,但 3D viewer 一片空白。

**原因**:Fixed Frame 沒對 — 預設可能是 `map`,但手臂只發 `world → base_link` TF。

**解**:RViz 左側 Global Options → Fixed Frame 改成 `world` 或 `base_link`。

### 雷 4:ompl_planning.yaml 內 default_planner_request_adapters 順序錯

**症狀**:Plan 出 trajectory 但 execute 時 controller 拒絕,log:「`Trajectory's first point doesn't match current state`」。

**解**:檢查 `ompl_planning.yaml` 內 `default_planner_request_adapters` 陣列,**`AddTimeOptimalParameterization` 必須在 `FixWorkspaceBounds` 之後**。Setup Assistant 預設正確,但若手動改過就要注意。

### 雷 5:重新編輯時改不到 SRDF 內容

**症狀**:用 `setup_assistant.launch.py` 重開 wizard 改了 group,Save 後沒生效。

**原因**:Setup Assistant 產出後,SRDF 直接寫在 `config/<arm>.srdf`,不是 URDF 旁邊那個。

**解**:重新 Generate Package,讓它覆蓋 `config/<arm>.srdf`。

---

## 👣 下一步去哪？

- [Phase 22B MoveIt 2 C++ 程式控制](../phase-22B-moveit-cpp/) — 用本章產出的 config 跑 `MoveGroupInterface` 規劃
- 想做完整 pick & place:Phase 23B(尚未完成)

---

> **驗證狀態**:⏸ 純文字草稿 — 流程依官方 Setup Assistant Humble 文件 + Phase 22B 經驗整理。截圖待 gino 在本機跑完 wizard 後補(`images/*.png` 預留位置已標)。執行驗證後升 ✅ WSL 完整驗證。
