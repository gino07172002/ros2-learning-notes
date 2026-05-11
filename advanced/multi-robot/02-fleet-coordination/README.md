# 02. Fleet Coordination — 任務分派 + 衝突避免

> 將多台獨立運作的機器人升級為一個「車隊 (Fleet)」。寫一個中央控制的 Fleet Manager，統籌分派任務並避免機器人互相碰撞。這就是 Amazon Kiva 或各大 AGV 倉儲系統的最核心大腦。

**學完你會**:
- 實作 `FleetManager` Node：訂閱全場機器人的位置，並接受使用者的任務佇列
- 寫出任務分派演算法（貪婪演算法：永遠指派離目標最近且空閒的機器人）
- 使用自訂的 Service (`AssignTask.srv`) 強制指派任務給特定的機器人
- 使用自訂的 Message (`FleetStatus.msg`) 定期向外廣播整個車隊的健康度與任務進度
- 處理多機系統中最經典的「Race Condition (競態條件)」

**前置**:
- [Phase 08 Custom Interfaces](../../../phase-08-custom-interfaces/) — 自訂通訊格式
- [Phase 13 Action](../../../phase-13-action/) — 呼叫 Nav2 的 `NavigateToPose`
- [Phase 01 Namespace + Spawn](../01-namespace-spawn/) — 確保你的多台機器人已經能在 Gazebo 中各自獨立運行

**產出**:
- [`code/fleet_manager_pkg/`](code/fleet_manager_pkg/) — 包含 Fleet Manager 邏輯與自訂 Message/Service 的 ROS 2 Package

**環境**:☁️ TheConstructSim / 💻 本機 WSL2 (需注意 CPU 負載)

---

## 📍 為什麼這章重要

單獨叫一台機器人走到 A 點很容易（Phase 22A），把 3 台機器人放在同一個地圖上也不難（Phase 01），但如果你有 10 個貨物要搬，**誰去搬哪一個？** 兩台車在走道上相會時**誰要讓誰？**

這些問題無法由單一機器人自己決定，必須要有個「上帝視角」的 `FleetManager` 來統籌。學會寫 Fleet Manager，代表你已經從「單機工程師」晉升到了「系統架構師」的層級。

---

## 💻 步驟 1: 定義通訊介面 (Custom Interfaces)

完整檔案見 [`code/fleet_interfaces/`](code/fleet_interfaces/)。

**FleetStatus.msg** (廣播全車隊狀態)：
```text
# 記錄單一機器人狀態
string robot_id
string current_task
geometry_msgs/Pose current_pose
float32 battery_level

---
# 陣列形式廣播所有機器人
FleetRobotStatus[] robots
```

**AssignTask.srv** (由外部系統下發任務)：
```text
string target_robot_id
geometry_msgs/PoseStamped target_pose
---
bool success
string message
```

---

## 💻 步驟 2: 實作 Fleet Manager (Python)

這是一個中央控制大腦。我們用 Python 來寫，因為處理字典與狀態機比較方便。

完整程式碼見 [`code/fleet_manager_pkg/fleet_manager_pkg/manager_node.py`](code/fleet_manager_pkg/fleet_manager_pkg/manager_node.py)。

```python
import rclpy
from rclpy.node import Node
from geometry_msgs.msg import PoseStamped
from nav2_msgs.action import NavigateToPose
from rclpy.action import ActionClient
# 假設你已經編譯了自訂介面
# from fleet_interfaces.srv import AssignTask
# from fleet_interfaces.msg import FleetStatus

class FleetManager(Node):
    def __init__(self):
        super().__init__('fleet_manager')
        
        # 假設我們有 3 台機器人
        self.robots = ['tb1', 'tb2', 'tb3']
        self.robot_states = {r: 'IDLE' for r in self.robots}
        
        # 為每台機器人建立 Nav2 Action Client
        self.nav_clients = {}
        for r in self.robots:
            action_name = f'/{r}/navigate_to_pose'
            self.nav_clients[r] = ActionClient(self, NavigateToPose, action_name)
        
        # 提供 Service 讓外部指派任務
        # self.srv = self.create_service(AssignTask, 'assign_task', self.assign_task_callback)

        self.get_logger().info("Fleet Manager is ready.")

    def assign_task_callback(self, request, response):
        robot_id = request.target_robot_id
        
        if robot_id not in self.robots:
            response.success = False
            response.message = f"Robot {robot_id} not found."
            return response
            
        if self.robot_states[robot_id] != 'IDLE':
            response.success = False
            response.message = f"Robot {robot_id} is busy."
            return response
            
        # 狀態機切換為 BUSY，並呼叫 Nav2
        self.robot_states[robot_id] = 'BUSY'
        self.send_nav_goal(robot_id, request.target_pose)
        
        response.success = True
        response.message = "Task assigned successfully."
        return response

    def send_nav_goal(self, robot_id, pose):
        client = self.nav_clients[robot_id]
        if not client.wait_for_server(timeout_sec=3.0):
            self.get_logger().error(f"Action server for {robot_id} not available.")
            self.robot_states[robot_id] = 'IDLE'
            return
            
        goal_msg = NavigateToPose.Goal()
        goal_msg.pose = pose
        
        # 異步發送 Goal
        send_goal_future = client.send_goal_async(goal_msg)
        send_goal_future.add_done_callback(
            lambda future: self.goal_response_callback(future, robot_id))

    def goal_response_callback(self, future, robot_id):
        goal_handle = future.result()
        if not goal_handle.accepted:
            self.get_logger().info(f"Task rejected by {robot_id}")
            self.robot_states[robot_id] = 'IDLE'
            return
            
        self.get_logger().info(f"Task accepted by {robot_id}")
        get_result_future = goal_handle.get_result_async()
        get_result_future.add_done_callback(
            lambda future: self.get_result_callback(future, robot_id))

    def get_result_callback(self, future, robot_id):
        # 任務完成，將機器人標記回 IDLE
        result = future.result().result
        self.get_logger().info(f"Robot {robot_id} finished task.")
        self.robot_states[robot_id] = 'IDLE'

def main(args=None):
    rclpy.init(args=args)
    node = FleetManager()
    rclpy.spin(node)
    node.destroy_node()
    rclpy.shutdown()

if __name__ == '__main__':
    main()
```

---

## 🚀 步驟 3: 跑 Demo

1. 先按照 `01-namespace-spawn` 的進度，將 3 台帶有獨立 Nav2 堆疊的 Turtlebot 啟動在同一個 Gazebo 世界中。
2. 編譯並啟動 Fleet Manager：
   ```bash
   colcon build --packages-select fleet_manager_pkg
   source install/setup.bash
   ros2 run fleet_manager_pkg manager_node
   ```
3. 從命令列手動呼叫 Service 指派任務（例如派 `tb1` 去某個座標）：
   ```bash
   ros2 service call /assign_task fleet_interfaces/srv/AssignTask "{target_robot_id: 'tb1', target_pose: {header: {frame_id: 'map'}, pose: {position: {x: 2.0, y: 2.0}, orientation: {w: 1.0}}}}"
   ```
4. 觀察終端機日誌，你會看到 `tb1` 接受任務、移動，並在到達目標後回到 `IDLE` 狀態。接著你可以指派 `tb2` 和 `tb3` 執行不同的任務。

---

## 🐛 常見雷

### ⚠️ 雷 1：任務分派 Race Condition

**症狀**：兩個節點幾乎同時呼叫 `/assign_task`，兩者都看到 `tb1` 是 IDLE，結果 `tb1` 的 Nav2 同時收到了兩個互相衝突的 Goal。
**原因**：ROS 2 Python Service Callback 預設是多執行緒執行的（如果用了 MultiThreadedExecutor）。
**解**：在 `self.robot_states` 的讀寫操作外層加上 Python 的 `threading.Lock()`，確保同一個時間點只有一個請求能改變機器人的狀態。

### ⚠️ 雷 2：Action Client 找不到 Server

**症狀**：`Action server for tb1 not available.`
**原因**：Nav2 的啟動非常緩慢（包含 8 個 lifecycle node），而你的 Fleet Manager 啟動太快了。
**解**：務必在 `send_goal` 裡面使用 `wait_for_server()`，或者在 Fleet Manager 啟動時先檢查所有的 Action Server 是否已經連線，如果沒連線就不要提供 `/assign_task` 服務。

### ⚠️ 雷 3：兩台機器人在狹窄走道卡死 (Deadlock)

**症狀**：`tb1` 要去左邊，`tb2` 要去右邊，兩台車在走道中間相遇，互不相讓，直到 Nav2 觸發 Recovery Behavior 甚至報錯停止。
**原因**：這是一個極度經典的 multi-robot 問題。標準的 Nav2 Global Planner 不知道其他機器人的未來路徑，只把對方當成「臨時出現的動態障礙物」。當空間不足以繞過時，兩台車就會卡死。
**解**：
- **初階解法**：在 Fleet Manager 層面，不要同時把會經過同一條走道的任務派給兩台車。
- **高階解法**：引入 `Open-RMF` 或使用支援多機路徑規劃的演算法（如 Conflict-Based Search, CBS）。

---

## 🎯 學到的關鍵概念

- **State Machine (狀態機)**：每台機器人都必須有明確的狀態 (`IDLE`, `BUSY`, `ERROR`, `CHARGING`)，這是系統調度的基礎。
- **Asynchronous Execution (非同步執行)**：在 Fleet Manager 內呼叫 Action 必須使用 Async 方法，否則等待一台車走到目標的 30 秒內，整個 Fleet Manager 都會卡死，無法指派任務給其他車。

---

## 🌟 進階挑戰

1. **實作 Greedy 分派**：修改 Service，讓使用者**不指定**要哪台車，只要傳送 `target_pose`。Fleet Manager 自動算出哪一台 `IDLE` 的車離目標最近，並指派給它。
2. **斷線重連機制**：如果 `tb2` 走到一半當機了（Nav2 Action Aborted），Fleet Manager 應該能接住這個 Error，並將這個未完成的任務重新丟回 Queue 中，派給其他車輛。
3. **電池模擬**：在 FleetStatus 中加入電量消耗邏輯，當電量低於 20% 時，強制該台機器人的狀態變為 `CHARGING` 並自動導航回起始點。
