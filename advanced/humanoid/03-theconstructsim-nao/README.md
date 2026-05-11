# 03. TheConstructSim (Cloud) — 雲端人形機器人實戰

> 如果你的本機電腦沒有獨立顯卡跑不動 MuJoCo，或者你不想經歷痛苦的環境配置，TheConstructSim 提供了一個極佳的替代方案。我們將在這裡使用經過「官方魔改」的 Gazebo 與預設配置好的全身控制器，在網頁瀏覽器中直接控制 NAO 或 TALOS 人形機器人。

**學完你會**:
- 了解如何在 TheConstructSim 的 Public ROSjects 中尋找並複製現成的人形機器人環境。
- 明白雲端環境是如何透過調整 Gazebo 參數與外掛，繞過複雜的接觸動力學問題。
- 透過高階指令 (如 `cmd_vel` 或 Action) 驅動預先寫好的步行控制器 (Walking Controller)。

**前置**:
- 擁有 TheConstructSim 的免費帳號
- [Phase 13 Action](../../../phase-13-action/) — 許多步行控制器是透過 Action 觸發的

**環境**:☁️ 專為 TheConstructSim 雲端設計

---

## 📍 雲端人形機器人的運作原理

我們在 [01. MuJoCo Simulation](../01-mujoco-simulation/) 中提過，Gazebo 很難處理雙足的硬接觸。TheConstructSim 是怎麼做到的？

1. **參數特調 (Parameter Tuning)**：官方對 Gazebo 的 ODE 物理引擎進行了深度微調，特別是 `mu1`, `mu2` (摩擦力), `kp`, `kd` (接觸剛度與阻尼)，讓腳底板與地板的碰撞變得「軟」一些，避免數值爆炸。
2. **內建步行引擎 (Built-in Walking Engine)**：你不需要像 02 章那樣自己算 Jacobian 與重心。他們通常會在背景跑一個 C++ 的外掛 (例如 NaoQI 或是 PAL Robotics 的 walking controller)，這個外掛已經內建了 ZMP (零力矩點) 演算法。
3. **高階封裝**：你只需要下達 `/cmd_vel` (往前走) 或呼叫特定的 Action，底層引擎就會自動幫你算出 20 幾個關節的扭力並維持平衡。

---

## 💻 步驟 1: 尋找並複製 ROSject

TheConstructSim 是一個龐大的社群，最快的方法是直接借用官方或大神的現成環境：

1. 登入 [TheConstruct](https://app.theconstructsim.com/)。
2. 點擊左側導覽列的 **"ROSjects"**。
3. 切換到 **"Public"** 標籤頁。
4. 在搜尋框中輸入 **"NAO"** (軟銀的經典小雙足) 或是 **"TALOS"** (PAL Robotics 的全尺寸人形)。
5. 找到標記為 ROS 2 (例如 Humble 或 Galactic) 的 ROSject，點擊右側的 **"Copy"** 圖示，將其複製到你的私人空間。
6. 點擊 **"Run"** 啟動虛擬機。

---

## 🚀 步驟 2: 啟動與測試

*(以下指令可能會依據你複製的具體 ROSject 有所不同，請優先參考該 ROSject 內建的 Notebook 說明)*

通常，環境啟動後，你需要開啟終端機並 launch 模擬器：

```bash
# 啟動 TALOS 在 Gazebo 中的模擬
ros2 launch talos_gazebo talos_gazebo.launch.py
```

等 Gazebo 的網頁介面載入後，你就會看到一隻巨大的 TALOS 機器人站在地上。

### 觀察隱藏在幕後的 Controllers
你可以用我們在 Phase 18 學過的技巧，檢查有哪些控制器正在運行：
```bash
ros2 control list_controllers
```
你會看到類似 `walking_controller`, `head_controller`, `arm_controller` 等等。這證明了「全身控制」已經被拆解成幾個高階模組並在背景默默運作了。

---

## 🎮 步驟 3: 讓機器人走起來 (Python Node)

既然底層已經幫我們顧好平衡了，我們就可以把它當作一台輪型車 (Turtlebot) 來下達速度指令。

這是一個簡單的 Python Node 範例，示範如何發送 `Twist` 指令給人形機器人的步行控制器：

*(這段程式碼可放在你建立的 package 中)*

```python
import rclpy
from rclpy.node import Node
from geometry_msgs.msg import Twist
import time

class CloudHumanoidWalker(Node):
    def __init__(self):
        super().__init__('cloud_humanoid_walker')
        
        # 注意：Topic 名稱可能依機器人不同 (例如 /talos_controller/cmd_vel 或 /cmd_vel)
        self.cmd_pub = self.create_publisher(Twist, '/walking_controller/cmd_vel', 10)
        self.timer = self.create_timer(0.1, self.timer_callback)
        self.start_time = time.time()
        
        self.get_logger().info("開始控制人形機器人步行...")

    def timer_callback(self):
        msg = Twist()
        t = time.time() - self.start_time
        
        if t < 5.0:
            # 前 5 秒：往前直走
            msg.linear.x = 0.2  # 每秒 20 公分
            msg.angular.z = 0.0
            self.get_logger().info("往前走...")
        elif t < 10.0:
            # 5 到 10 秒：原地左轉
            msg.linear.x = 0.0
            msg.angular.z = 0.5 # 旋轉
            self.get_logger().info("左轉彎...")
        else:
            # 停下
            msg.linear.x = 0.0
            msg.angular.z = 0.0
            self.get_logger().info("停止。")
            self.timer.cancel()
            
        self.cmd_pub.publish(msg)

def main(args=None):
    rclpy.init(args=args)
    node = CloudHumanoidWalker()
    rclpy.spin(node)
    node.destroy_node()
    rclpy.shutdown()

if __name__ == '__main__':
    main()
```

執行這個 Node 後，切換到 Gazebo 畫面，你會看到 TALOS 機器人開始「踏步」，並穩穩地向前移動。它的雙手可能會自然擺動以維持平衡，這都是底層外掛的功勞。

---

## 🐛 常見雷

### ⚠️ 雷 1：給的速度太快，機器人直接摔倒
**症狀**：設定 `linear.x = 1.0` 後，機器人腳步跟不上，往前撲倒。
**原因**：雖然底層有平衡演算法，但每個機器人都有其極限步行速度（通常人形機器人走得很慢，大約 0.2 ~ 0.5 m/s）。超過這個極限，ZMP 演算法就無法將重心維持在腳掌內了。
**解**：循序漸進地增加速度，並參考該機器人的硬體規格表。

### ⚠️ 雷 2：關節報錯 `Effort limit exceeded`
**症狀**：機器人在舉手或蹲下時，突然停住，Console 噴出一堆紅字。
**原因**：URDF 中有定義每個馬達的最大扭力 (Effort Limit)。如果你的動作過於劇烈，或者讓機器人提起了太重的物體，需要的扭力就會超過極限，`ros2_control` 會為了保護馬達而緊急切斷輸出。
**解**：讓動作變得更平滑 (不要在 0.1 秒內要求轉動 90 度)。

---

## 🎯 總結：本機 vs 雲端 的選擇

- **選本機 (MuJoCo + WBC)**：如果你想成為「**控制演算法工程師**」，想親自寫數學公式來控制馬達扭矩、研究 RL 強化學習，你必須走這條硬核路線。
- **選雲端 (TheConstructSim)**：如果你想成為「**應用層工程師**」，想研究如何讓雙足機器人結合 LLM 大語言模型做自然語言交互、或是結合 Yolo 做視覺抓取，利用雲端現成的穩定底層是最高效的做法。
