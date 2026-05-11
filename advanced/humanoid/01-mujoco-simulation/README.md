# 01. MuJoCo Simulation — 高效能動態模擬

> 拋棄你對 Gazebo 的習慣。進入人形與雙足機器人的世界，我們需要一套能處理「極度頻繁且堅硬的地面碰撞」的物理引擎。歡迎來到 Google DeepMind 開源的 MuJoCo！

**學完你會**:
- 了解為什麼雙足機器人開發者偏愛 MuJoCo (Multi-Joint dynamics with Contact)。
- 安裝並使用 `mujoco_ros2_control` 將 MuJoCo 模擬器掛載進 ROS 2 系統。
- 在 MuJoCo 中載入 Unitree G1 (或其他 20+ DOF 人形機器人) 的模型檔。
- 啟動標準的 `ros2_control` 架構，透過發送 ROS Topic 讓機器人的各個關節動起來。

**前置**:
- [Phase 18 ros2_control](../../../phase-18-ros2-control/) — 你必須理解 controller_manager 是怎麼運作的
- [Phase 20B URDF](../../../phase-20B-arm-urdf/) — 機器人的骨架定義

**產出**:
- 透過指令啟動的 MuJoCo 模擬視窗，內含一隻站立的人形機器人，並能透過 ROS 2 接收指令。

**環境**:💻 本機 WSL2 / Ubuntu (MuJoCo 使用 OpenGL 渲染，無法在無圖形介面的純文字雲端環境中執行)

---

## 📍 為什麼不用 Gazebo？

在我們學輪式機器人 (TurtleBot) 或固定在桌上的機械手臂時，Gazebo 表現得非常好。但是，當我們進入**雙足步行**的領域時：

1. **接觸動力學 (Contact Dynamics)**：人形機器人每走一步，腳底板都會與地面發生劇烈的高頻碰撞。Gazebo (底層通常是 ODE 或 Bullet) 在處理這類硬接觸 (Hard Contact) 時，常常會導致穿模、數值發散，甚至讓機器人無緣無故飛上天。
2. **模擬速度**：訓練強化學習 (RL) 演算法來教機器人走路，需要模擬器運行得比真實時間快千百倍。MuJoCo 是專門為此設計的，它放棄了部分華麗的光影渲染，追求極致的物理運算速度與穩定性。

因此，**MuJoCo + ROS 2 已經成為現代人形機器人開發的黃金組合**。

---

## 💻 步驟 1: 認識 MJCF 與 URDF

ROS 2 的世界只認得 `URDF`，它用來發布 TF (座標轉換)。但是，MuJoCo 有自己專屬的 XML 格式叫做 **MJCF** (`.xml`)。

這意味著一個標準的人形機器人 Package 裡面通常會有兩種模型檔：
- `robot.urdf`：餵給 `robot_state_publisher`，負責告訴 RViz 手腳長在哪裡。
- `scene.xml` (MJCF)：餵給 MuJoCo，負責物理模擬、馬達出力限制、與地面的摩擦力係數。

在準備機器人模型時（例如去下載 Unitree 官方開源的 G1 模型），確保這兩個檔案的關節名稱 (Joint Names) 是 100% 一致的，這樣 `ros2_control` 才能成功幫我們在兩個世界中搭起橋樑。

---

## ⚙️ 步驟 2: 整合 `mujoco_ros2_control`

這是一個神奇的橋接外掛。在你的 MJCF 檔案中，只要加入以下這個 `<extension>` 標籤，MuJoCo 啟動時就會自動召喚出 ROS 2 的 `controller_manager`！

編輯你的 `scene.xml`：
```xml
<mujoco>
  <!-- 引入機器人本體的定義 -->
  <include file="g1.xml"/>

  <!-- 啟動 ROS 2 Control 外掛 -->
  <extension>
    <plugin plugin="mujoco_ros2_control">
      <!-- 告訴外掛去哪裡找你的 ros2_control YAML 設定檔 -->
      <parameters>$(find my_humanoid_config)/config/ros2_controllers.yaml</parameters>
    </plugin>
  </extension>
</mujoco>
```

---

## 💻 步驟 3: 設定 ros2_control YAML

人形機器人動輒 20~30 個關節，手寫 YAML 會非常壯觀。為了簡單起見，我們通常把所有的手腳關節包在一個 `forward_command_controller` 或 `joint_trajectory_controller` 裡面。

建立 `ros2_controllers.yaml`：
```yaml
controller_manager:
  ros__parameters:
    update_rate: 1000  # 人形機器人的控制頻率通常高達 1000 Hz

    joint_state_broadcaster:
      type: joint_state_broadcaster/JointStateBroadcaster

    # 全身關節控制器 (這裡以位置控制為例)
    whole_body_position_controller:
      type: position_controllers/JointGroupPositionController

whole_body_position_controller:
  ros__parameters:
    joints:
      - left_hip_pitch_joint
      - left_hip_roll_joint
      - left_hip_yaw_joint
      - left_knee_joint
      - left_ankle_pitch_joint
      - left_ankle_roll_joint
      # ... (省略另外十幾個關節)
```

---

## 🚀 步驟 4: 跑 Demo (概念指令)

編寫一個 Launch 檔，把所有的東西開起來：

```bash
# 1. 啟動 MuJoCo 模擬器，並帶入包含 ros2_control plugin 的 MJCF 檔
# (底層會自動載入 controller_manager)
ros2 launch my_humanoid_config start_mujoco.launch.py

# 2. 啟動 RViz，看看 ROS 2 世界裡的 TF 有沒有正確對齊 MuJoCo 世界
ros2 launch my_humanoid_config view_robot.launch.py

# 3. 載入並啟動 Controllers
ros2 control load_controller --set-state active joint_state_broadcaster
ros2 control load_controller --set-state active whole_body_position_controller
```

開啟新的終端機，送一個命令讓機器人舉起手或彎曲膝蓋：
```bash
ros2 topic pub /whole_body_position_controller/commands std_msgs/msg/Float64MultiArray "{data: [0.0, 0.0, 0.0, -0.5, 0.0, 0.0, ...]}" --once
```
切換到 MuJoCo 的視窗，你會看到機器人瞬間依照你的指令改變了姿勢。如果重心不穩，它會非常真實地「摔倒在地上」。

---

## 🐛 常見雷

### ⚠️ 雷 1：機器人一出生就癱軟在地上
**症狀**：MuJoCo 啟動後，機器人像洩了氣的皮球一樣癱在地上。
**原因**：你沒有啟動任何 Controller 來支撐馬達的位置。在真實世界中，斷電的馬達就是沒有扭力 (Torque) 的。
**解**：務必確保 `whole_body_position_controller` 成功變為 `active` 狀態，並且預設發送了一組「站立姿態」的起始點。

### ⚠️ 雷 2：MuJoCo 與 ROS 2 的時間不同步
**症狀**：TF 更新的頻率跟不上 MuJoCo 畫面，或是 Controller 的 PID 計算爆掉。
**原因**：MuJoCo 內部運算可能遠高於即時時間 (Real-time)。
**解**：在 Launch 檔中確保有設定 `use_sim_time:=true` 給所有 ROS 2 節點，讓 ROS 2 乖乖聽從 MuJoCo plugin 廣播出來的 `/clock`。

### ⚠️ 雷 3：WSL2 中打不開 MuJoCo 視窗
**症狀**：出現 `Failed to initialize OpenGL` 或 `Wayland/X11 error`。
**解**：MuJoCo 對圖形驅動要求較高。請確保你的 WSL2 是最新版，並且有安裝好 Windows 的 GPU 驅動。可以先在 WSL 終端機跑 `glxgears` 測試圖形介面是否正常運作。

---

## 🎯 學到的關鍵概念

- **MJCF 格式**：專為物理與接觸最佳化的機器人描述檔。
- **高頻控制 (High-frequency Control)**：在輪式機器人我們可能 50Hz 就夠了，但雙足的平衡運算往往需要 1000Hz (1毫秒) 的反應速度，這極度考驗程式碼的效能。

---

## 🔗 下一步

把機器人叫出來只是第一步！接下來我們不能只靠手動發送 `Float64MultiArray`，這太反人類了。進入 [02. Whole-Body Control](../02-whole-body-control/)，我們將撰寫演算法來協調幾十個關節，讓機器人做出一套流暢的動作！
