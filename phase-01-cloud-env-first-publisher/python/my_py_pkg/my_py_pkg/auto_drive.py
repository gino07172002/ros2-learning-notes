import rclpy
from rclpy.node import Node
from geometry_msgs.msg import Twist


class AutoDriveNode(Node):
    def __init__(self):
        super().__init__('auto_drive_node')

        self.publisher = self.create_publisher(Twist, 'cmd_vel', 10)

        self.timer = self.create_timer(0.5, self.timer_callback)
        self.start_time = self.get_clock().now()

    def timer_callback(self):
        msg = Twist()
        elapsed = (self.get_clock().now() - self.start_time).nanoseconds / 1e9

        if elapsed < 3.0:
            msg.linear.x = 0.2
            msg.angular.z = 0.0
        else:
            msg.linear.x = 0.0
            msg.angular.z = 0.0

        self.publisher.publish(msg)


def main(args=None):
    rclpy.init(args=args)
    node = AutoDriveNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
