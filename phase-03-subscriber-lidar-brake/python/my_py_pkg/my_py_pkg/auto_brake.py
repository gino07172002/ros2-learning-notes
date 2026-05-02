import rclpy
from rclpy.node import Node
from rclpy.qos import qos_profile_sensor_data
from geometry_msgs.msg import Twist
from sensor_msgs.msg import PointCloud2
from sensor_msgs_py import point_cloud2


class AutoBrakeNode(Node):
    def __init__(self):
        super().__init__('auto_brake_node')

        self.publisher = self.create_publisher(Twist, 'cmd_vel', 10)

        self.subscription = self.create_subscription(
            PointCloud2,
            'lidar_points',
            self.cloud_callback,
            qos_profile_sensor_data,
        )

        self.get_logger().info('3D Auto Brake Started!')

    def cloud_callback(self, msg: PointCloud2):
        twist = Twist()
        min_forward_distance = 100.0

        points = point_cloud2.read_points(
            msg, field_names=('x', 'y'), skip_nans=True
        )

        for x, y in points:
            if x > 0.0 and abs(y) < 0.2:
                if x < min_forward_distance:
                    min_forward_distance = float(x)

        if min_forward_distance > 1.0:
            twist.linear.x = 0.2
            twist.angular.z = 0.0
            self.get_logger().info(
                f'Clear ahead (Closest: {min_forward_distance:.2f}m). Moving forward...',
                throttle_duration_sec=1.0,
            )
        else:
            twist.linear.x = 0.0
            twist.angular.z = 0.0
            self.get_logger().warn(
                f'Obstacle detected at {min_forward_distance:.2f}m! BRAKING!',
                throttle_duration_sec=1.0,
            )

        self.publisher.publish(twist)


def main(args=None):
    rclpy.init(args=args)
    node = AutoBrakeNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
