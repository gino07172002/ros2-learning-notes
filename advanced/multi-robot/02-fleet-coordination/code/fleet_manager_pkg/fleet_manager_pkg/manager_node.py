import rclpy
from rclpy.node import Node
from geometry_msgs.msg import PoseStamped
from nav2_msgs.action import NavigateToPose
from rclpy.action import ActionClient
import threading

class FleetManager(Node):
    def __init__(self):
        super().__init__('fleet_manager')
        
        self.robots = ['tb1', 'tb2', 'tb3']
        self.robot_states = {r: 'IDLE' for r in self.robots}
        
        self.nav_clients = {}
        for r in self.robots:
            action_name = f'/{r}/navigate_to_pose'
            self.nav_clients[r] = ActionClient(self, NavigateToPose, action_name)
        
        # 加上 Lock 防止 Race Condition
        self.state_lock = threading.Lock()

        self.get_logger().info("Fleet Manager is ready. Use service (or test directly via code) to assign tasks.")

    def assign_task_callback(self, request, response):
        robot_id = request.target_robot_id
        
        with self.state_lock:
            if robot_id not in self.robots:
                response.success = False
                response.message = f"Robot {robot_id} not found."
                return response
                
            if self.robot_states[robot_id] != 'IDLE':
                response.success = False
                response.message = f"Robot {robot_id} is busy."
                return response
                
            self.robot_states[robot_id] = 'BUSY'
            
        self.send_nav_goal(robot_id, request.target_pose)
        
        response.success = True
        response.message = "Task assigned successfully."
        return response

    def send_nav_goal(self, robot_id, pose):
        client = self.nav_clients[robot_id]
        if not client.wait_for_server(timeout_sec=3.0):
            self.get_logger().error(f"Action server for {robot_id} not available.")
            with self.state_lock:
                self.robot_states[robot_id] = 'IDLE'
            return
            
        goal_msg = NavigateToPose.Goal()
        goal_msg.pose = pose
        
        send_goal_future = client.send_goal_async(goal_msg)
        send_goal_future.add_done_callback(
            lambda future: self.goal_response_callback(future, robot_id))

    def goal_response_callback(self, future, robot_id):
        goal_handle = future.result()
        if not goal_handle.accepted:
            self.get_logger().info(f"Task rejected by {robot_id}")
            with self.state_lock:
                self.robot_states[robot_id] = 'IDLE'
            return
            
        self.get_logger().info(f"Task accepted by {robot_id}")
        get_result_future = goal_handle.get_result_async()
        get_result_future.add_done_callback(
            lambda future: self.get_result_callback(future, robot_id))

    def get_result_callback(self, future, robot_id):
        # 任務完成
        result = future.result().result
        self.get_logger().info(f"Robot {robot_id} finished task.")
        with self.state_lock:
            self.robot_states[robot_id] = 'IDLE'

def main(args=None):
    rclpy.init(args=args)
    node = FleetManager()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()

if __name__ == '__main__':
    main()
