# 02. Whole-Body Control (WBC) — 全身控制基礎

> 如果你只是想讓機器的「右手」去抓杯子，用 MoveIt 算一算手臂的 6 個關節角度就好。但如果你是一隻雙足機器人，當你的右手往前伸時，你的「重心 (CoM)」會前傾，如果不把上半身往後仰或是腳步往前踏，你就會直接摔個狗吃屎。這就是為什麼我們需要 Whole-Body Control！

**學完你會**:
- 了解為什麼雙足機器人不能只用單純的「手臂 IK (Inverse Kinematics)」，而必須考慮「動力學 (Dynamics)」與「重心 (Center of Mass)」。
- 認識剛體動力學的業界標準函式庫 **Pinocchio**。
- 寫一個 ROS 2 Node，計算出讓機器人「平穩下蹲」的全身關節軌跡。
- 透過上一章設定的 `ros2_control` 介面，將算好的軌跡下發給 MuJoCo 模擬器執行。

**前置**:
- [01. MuJoCo Simulation](../01-mujoco-simulation/) — 你需要一個能在模擬器中站立的人形機器人。
- [Phase 18 ros2_control](../../../phase-18-ros2-control/) — 控制介面。

**產出**:
- [`code/wbc_demo/`](code/wbc_demo/) — 一個結合 Pinocchio 進行重心計算，並發佈全身關節指令的 Python 套件。

**環境**:💻 本機 WSL2 / Ubuntu

---

## 📍 什麼是 WBC (Whole-Body Control)？

在傳統機械手臂 (Phase 21B, 22B) 中，底座是鎖死在桌子或地上的。你只要管手臂會不會撞到東西就好。

但在人形機器人中，**底座 (Base) 處於漂浮狀態 (Floating Base)**。這衍生出兩個致命問題：
1. **多餘自由度 (Redundancy)**：當你想讓右手往前伸 10 公分，你可以只轉動肩膀，也可以彎曲腰部，甚至可以彎曲膝蓋。有無限多種關節組合能達到同一個手部位置。
2. **接觸力與平衡**：如果你伸手的動作太猛，或者手上的物體太重，機器人的投影重心 (CoM) 超出了腳底板的支撐多邊形 (Support Polygon)，機器人就會跌倒。

WBC 就是一個巨大的**最佳化問題 (Optimization Problem)**：
> 「請在保證『雙腳不離開地面』且『重心不超出腳底』的前提下，尋找一組最省力的 20 個關節扭矩 (Torque)，讓右手到達目標位置。」

---

## 💻 步驟 1: 認識 Pinocchio 函式庫

要解決 WBC 的最佳化問題，程式必須知道機器人身上每一塊鐵疙瘩的重量、長度、慣性矩。這就是 **Pinocchio** 函式庫發揮作用的地方。

Pinocchio 是由法國 INRIA 開發的開源剛體動力學函式庫，速度極快，是業界計算 Jacobian 矩陣、正/逆向動力學的首選。

安裝 Pinocchio (Python 版)：
```bash
# Ubuntu 22.04 通常可以透過 APT 安裝
sudo apt install robotpkg-py310-pinocchio
# 或是透過 pip 安裝
pip install pin
```

---

## 💻 步驟 2: 撰寫下蹲控制 Node

在這個 Demo 中，我們不會手刻一個極度複雜的非線性最佳化求解器 (那是博士級別的研究)。我們將實作一個「最簡化的運動學下蹲」：
1. 利用 Pinocchio 載入機器人的 URDF。
2. 使用簡單的幾何學計算出雙腿各關節（髖、膝、踝）為了降低骨盆高度所需的角度變化。
3. 產生一條平滑的軌跡，發送給 `ros2_control`。

完整程式碼見 [`code/wbc_demo/wbc_demo/squat_controller.py`](code/wbc_demo/wbc_demo/squat_controller.py)。

```python
import rclpy
from rclpy.node import Node
from std_msgs.msg import Float64MultiArray
import time
import math
import numpy as np
# 載入 pinocchio
import pinocchio as pin

class SquatController(Node):
    def __init__(self):
        super().__init__('squat_controller')
        
        # 建立 Publisher，將指令發給上一章設定的 joint_group_position_controller
        self.cmd_pub = self.create_publisher(
            Float64MultiArray, 
            '/whole_body_position_controller/commands', 
            10
        )

        # 載入機器人的 URDF 到 Pinocchio 模型中
        # (這裡假設你手邊有機器人的 URDF 檔案路徑)
        # urdf_path = "/path/to/your/humanoid.urdf"
        # self.model = pin.buildModelFromUrdf(urdf_path)
        # self.data = self.model.createData()

        self.get_logger().info("Squat Controller Initialized.")
        
        # 為了展示，我們直接產生一條正弦波 (Sine wave) 軌跡讓膝蓋彎曲
        self.timer = self.create_timer(0.01, self.control_loop) # 100 Hz
        self.start_time = time.time()

    def control_loop(self):
        t = time.time() - self.start_time
        
        # 產生一個週期為 4 秒的平滑下蹲軌跡
        # 假設 0 是站直，-0.5 弧度是下蹲深度
        squat_depth = -0.5 * (1.0 - math.cos(t * math.pi / 2.0)) / 2.0
        
        # 在真實的 WBC 中，這裡會呼叫最佳化求解器，
        # 計算出維持 CoM 不變的情況下，全身 20 個關節應該有的角度或扭矩。
        
        # 這裡作為極簡 Demo，我們手動對應雙腿的髖(hip)、膝(knee)、踝(ankle)關節。
        # (數值與方向會依照每台機器人的 URDF 定義有所不同，這只是一個概念演示)
        hip_pitch = -squat_depth
        knee = 2.0 * squat_depth  # 膝蓋彎曲的角度通常是髖部的兩倍
        ankle_pitch = -squat_depth

        # 組合出要發給 ros2_control 的陣列
        # 必須與 ros2_controllers.yaml 中定義的關節順序完全一致
        msg = Float64MultiArray()
        # 假設順序是: 左髖, 左膝, 左踝, 右髖, 右膝, 右踝, ... (省略手部)
        msg.data = [
            hip_pitch, knee, ankle_pitch, 
            hip_pitch, knee, ankle_pitch,
            0.0, 0.0, 0.0, 0.0 # 其他關節保持 0
        ]
        
        self.cmd_pub.publish(msg)

def main(args=None):
    rclpy.init(args=args)
    node = SquatController()
    rclpy.spin(node)
    node.destroy_node()
    rclpy.shutdown()

if __name__ == '__main__':
    main()
```

---

## 🚀 步驟 3: 跑 Demo

1. 確保你已經啟動了上一章的 MuJoCo 模擬器與 `ros2_control`：
   ```bash
   # (在終端機 1)
   ros2 launch my_humanoid_config start_mujoco.launch.py
   ```
2. 編譯並啟動我們的下蹲 Node：
   ```bash
   # (在終端機 2)
   cd ~/ros2_ws
   colcon build --packages-select wbc_demo
   source install/setup.bash
   ros2 run wbc_demo squat_controller
   ```
3. 切換回 MuJoCo 視窗，你應該會看到機器人像是在做深蹲運動一樣，隨著正弦波的頻率，平滑地彎曲膝蓋然後站直。

---

## 🐛 常見雷

### ⚠️ 雷 1：下蹲時重心不穩往前摔倒
**症狀**：機器人膝蓋彎曲了，但隨即失去平衡往前仆街。
**原因**：我們的簡易 Demo 只使用了直覺的幾何學彎曲關節，但沒有精確計算上半身的重量比例。當膝蓋彎曲時，如果不把上半身往後仰（或腳踝補償），整體的重心 (CoM) 會移出腳掌的範圍。
**解**：這就是為什麼真實開發必須依賴 Pinocchio 計算 `pin.centerOfMass(model, data, q)`。你必須在控制迴圈中確保 CoM 的 X, Y 座標永遠落在左腳與右腳構成的多邊形之內。

### ⚠️ 雷 2：關節陣列順序錯亂
**症狀**：送出指令後，機器人的左手突然往後折，或者腳踝朝奇怪的方向扭曲。
**原因**：`Float64MultiArray` 是沒有 Key (鍵名) 的，它純粹依賴陣列的順序。如果你傳送的順序與 `ros2_controllers.yaml` 中的 `joints` 列表不一致，就會控制到錯誤的馬達。
**解**：強烈建議在程式啟動時，先訂閱一次 `/joint_states`，讀取裡面的 `name` 陣列，建立一個正確的 Index 對應表，再按照這個順序填入 `msg.data`。

### ⚠️ 雷 3：控制頻率太低導致抽搐
**症狀**：動作看起來一卡一卡的。
**原因**：雙足機器人的動態平衡對時間極度敏感。如果你的 Python 腳本因為 Garbage Collection 或 CPU 負載導致控制頻率從 100Hz 掉到 20Hz，物理引擎會得到非常離散的指令。
**解**：在業界的 production 代碼中，WBC 幾乎 100% 都是用 **C++** 撰寫，並且可能會設定為 RT (Real-Time) 優先級執行緒。

---

## 🎯 學到的關鍵概念

- **Floating Base (浮動基座)**：人形機器人的第一關。底座不再是固定點，這讓所有的運動學計算都增加了一層「維持平衡」的物理限制。
- **Pinocchio**：處理 URDF 並高速計算動力學矩陣的開源神器。不用它，你自己手算 Jacobian 會算到崩潰。
- **WBC 的本質**：WBC 不是單一的演算法，而是一個系統工程。它通常包含一個二次規劃 (QP, Quadratic Programming) 求解器，將「我想讓右手到達 (x,y,z)」化為目標函數，並將「重心不出界」、「馬達扭力不超過極限」化為限制條件，瞬間算出所有的關節目標值。

---

## 🌟 進階挑戰

1. **整合 Pinocchio 算 CoM**：嘗試在 `squat_controller.py` 中載入你機器人的 URDF，並在每一次迴圈印出目前的 CoM 座標。
2. **力矩控制 (Torque Control)**：目前我們是用「位置控制 (Position Control)」，這代表機器人非常僵硬。嘗試在 `ros2_control` 中切換成 `effort_controllers/JointGroupEffortController`，並發送扭矩 (Torque) 指令。你會發現這難度高上十個量級，因為只要扭力少給了一點點，機器人就會軟腳！

---

## 🔗 下一步

人形機器人的這兩章只是為你打開了這扇大門。目前業界最火熱的解法，是將這種傳統的 WBC 控制與 **強化學習 (RL, Reinforcement Learning)** 結合。用 RL 在模擬器中跌倒幾百萬次訓練出一個神經網路，然後直接把網路輸出轉成關節指令。
你可以探索像是 [Isaac Gym](https://developer.nvidia.com/isaac-gym) 等工具，開始你的 AI 機器人訓練之旅！
