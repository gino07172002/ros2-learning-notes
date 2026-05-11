import rclpy
from rclpy.node import Node
from std_msgs.msg import Float64MultiArray
import time
import math
import numpy as np
# 如果有安裝 pinocchio，可以取消註解以下行來載入 URDF
# import pinocchio as pin

class SquatController(Node):
    def __init__(self):
        super().__init__('squat_controller')
        
        # 建立 Publisher，將指令發給 controller_manager
        self.cmd_pub = self.create_publisher(
            Float64MultiArray, 
            '/whole_body_position_controller/commands', 
            10
        )

        self.get_logger().info("Squat Controller Initialized. Starting squat motion...")
        
        # 為了展示，我們直接產生一條正弦波軌跡讓膝蓋彎曲
        self.timer = self.create_timer(0.01, self.control_loop) # 100 Hz
        self.start_time = time.time()

    def control_loop(self):
        t = time.time() - self.start_time
        
        # 產生一個平滑下蹲軌跡
        # 假設 0 是站直，-0.5 弧度是下蹲深度
        squat_depth = -0.5 * (1.0 - math.cos(t * math.pi / 2.0)) / 2.0
        
        # 手動對應雙腿的髖(hip)、膝(knee)、踝(ankle)關節
        # 實際應用中這裡應該是由 WBC 求解器算出來的
        hip_pitch = -squat_depth
        knee = 2.0 * squat_depth  # 膝蓋彎曲的角度通常是髖部的兩倍
        ankle_pitch = -squat_depth

        # 組合出要發給 ros2_control 的陣列
        msg = Float64MultiArray()
        # 假設順序是: 左髖, 左膝, 左踝, 右髖, 右膝, 右踝
        # 注意：實際長度必須等於 yaml 中配置的關節數量
        msg.data = [
            hip_pitch, knee, ankle_pitch, 
            hip_pitch, knee, ankle_pitch,
            0.0, 0.0, 0.0, 0.0 # 假設還有 4 個手臂關節保持 0
        ]
        
        self.cmd_pub.publish(msg)

def main(args=None):
    rclpy.init(args=args)
    node = SquatController()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()

if __name__ == '__main__':
    main()
