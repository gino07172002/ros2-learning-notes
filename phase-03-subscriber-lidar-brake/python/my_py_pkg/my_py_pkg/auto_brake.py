import rclpy
from rclpy.node import Node
# qos_profile_sensor_data：ROS 2 預設的「感測器」QoS 組合
# (Best Effort + Volatile + KeepLast(5))。對齊大多數光達/相機的 publisher。
from rclpy.qos import qos_profile_sensor_data
from geometry_msgs.msg import Twist
from sensor_msgs.msg import PointCloud2
# sensor_msgs_py.point_cloud2：解析 PointCloud2 binary blob 的工具模組。
# 等同 C++ 版的 PointCloud2ConstIterator——不要自己手動 unpack bytes。
from sensor_msgs_py import point_cloud2


# 同一個 Node 同時是 Publisher（cmd_vel）+ Subscriber（光達）——
# 在 ROS 2 中很常見，因為 callback 共用 spin 的事件迴圈。
class AutoBrakeNode(Node):
    def __init__(self):
        super().__init__('auto_brake_node')

        self.publisher = self.create_publisher(Twist, 'cmd_vel', 10)

        # Subscriber 簽章：(訊息型別, topic 名, callback, qos)
        # 跟 paho-mqtt 的 subscribe 不同：訊息型別在訂閱時就決定了，
        # callback 收到的 msg 已經是反序列化好的 PointCloud2 物件，不是 raw bytes。
        self.subscription = self.create_subscription(
            PointCloud2,
            'lidar_points',
            self.cloud_callback,
            # ⚠️ QoS 必須對齊 publisher，否則「無聲失敗」——訊息在發但收不到。
            # 感測器資料一律用 qos_profile_sensor_data。
            qos_profile_sensor_data,
        )

        # 用 logger 不要用 print()——logger 會送到 /rosout topic，
        # 可被 rqt_console 看、ros2 bag 錄、被其他工具索引。
        self.get_logger().info('3D Auto Brake Started!')

    # type hint 純文件用途，rclpy 不會驗證；但寫了 IDE 自動補全與 mypy 都受惠。
    def cloud_callback(self, msg: PointCloud2):
        twist = Twist()
        min_forward_distance = 100.0  # 「無限遠」哨兵值

        # read_points 是 generator，逐點 yield (x, y) tuple。
        # field_names 限定只讀需要的欄位（省記憶體+省時間）。
        # skip_nans=True 過濾無效點（光達常會有 NaN）。
        # ⚠️ 對大點雲（>10 萬點），純 Python 迴圈會慢，改用 read_points_numpy 向量化。
        points = point_cloud2.read_points(
            msg, field_names=('x', 'y'), skip_nans=True
        )

        for x, y in points:
            # 篩選正前方 40cm 寬走廊：x>0 表示在前方，|y|<0.2 表示左右各 20cm 內
            if x > 0.0 and abs(y) < 0.2:
                if x < min_forward_distance:
                    # 從 numpy.float32 轉 Python float（之後 publish 時要的是純 float）
                    min_forward_distance = float(x)

        if min_forward_distance > 1.0:
            twist.linear.x = 0.2
            twist.angular.z = 0.0
            # throttle_duration_sec：rclpy 從 Iron 版起的內建限流 API。
            # 光達回呼約 10Hz，沒節流會讓 terminal 洗版。
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

        # subscriber callback 內 publish 是常見的 reactive control 模式。
        # callback 不要做太多事，否則 spin 卡住其他 callback。
        self.publisher.publish(twist)


def main(args=None):
    rclpy.init(args=args)
    node = AutoBrakeNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        # rclpy 對 Ctrl+C 不會自動吞掉，要自己 catch 才不會印 traceback
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
