#!/usr/bin/env python3
# fake_lidar.py — 發 PointCloud2 假資料給 ApproachController 吃
#
# 跟 ~/fake_lidar.py(WSL 用)是同一份,放這裡讓 container 內也有一份
# 可以 docker compose run lidar 客製距離

import sys
import struct
import rclpy
from rclpy.node import Node
from rclpy.qos import qos_profile_sensor_data
from sensor_msgs.msg import PointCloud2, PointField


class FakeLidar(Node):
    def __init__(self, distance: float):
        super().__init__('fake_lidar')
        self.distance = distance
        self.pub = self.create_publisher(
            PointCloud2, 'lidar_points', qos_profile_sensor_data
        )
        self.timer = self.create_timer(0.1, self.tick)
        self.get_logger().info(
            f'Publishing fake PointCloud2 with obstacle at {distance}m forward'
        )

    def tick(self):
        msg = PointCloud2()
        msg.header.frame_id = 'lidar'
        msg.header.stamp = self.get_clock().now().to_msg()

        x, y, z = float(self.distance), 0.0, 0.0
        data = struct.pack('fff', x, y, z)

        msg.height = 1
        msg.width = 1
        msg.fields = [
            PointField(name='x', offset=0, datatype=PointField.FLOAT32, count=1),
            PointField(name='y', offset=4, datatype=PointField.FLOAT32, count=1),
            PointField(name='z', offset=8, datatype=PointField.FLOAT32, count=1),
        ]
        msg.is_bigendian = False
        msg.point_step = 12
        msg.row_step = 12
        msg.data = data
        msg.is_dense = True
        self.pub.publish(msg)


def main():
    distance = float(sys.argv[1]) if len(sys.argv) > 1 else 0.5
    rclpy.init()
    node = FakeLidar(distance)
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
