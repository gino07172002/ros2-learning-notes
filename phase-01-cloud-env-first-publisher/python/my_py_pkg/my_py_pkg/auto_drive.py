import rclpy
from rclpy.node import Node
# Twist 訊息：標準速度指令格式（linear.x/y/z + angular.x/y/z）。
# rclpy 的訊息類別由 IDL 自動產生，欄位都是 float，會做型別檢查。
from geometry_msgs.msg import Twist


# 在 ROS 2 中「節點 (Node)」= 一個獨立執行單位，類比 paho-mqtt 的 Client 物件。
# 慣例做法：繼承 rclpy.node.Node 並把 publisher / timer / subscriber 都掛在 self 上。
class AutoDriveNode(Node):
    def __init__(self):
        # 'auto_drive_node' 是節點名，會出現在 ros2 node list / rqt_graph / log 中。
        # 等同 MQTT client_id，必須在系統內唯一。
        super().__init__('auto_drive_node')

        # 建立 Publisher：類比 mqtt_client.subscribe() 的反向操作（先註冊一個發送通道）。
        # - Twist：訊息類別（強型別，不像 MQTT 是 raw bytes）
        # - 'cmd_vel'：相對 topic 名，執行時可由 launch / CLI remap 重新對應
        # - 10：佇列深度（QoS history depth，類比 MQTT QoS 1 的 in-flight 上限）
        self.publisher = self.create_publisher(Twist, 'cmd_vel', 10)

        # create_timer：Node 內建計時器，每 0.5 秒呼叫 self.timer_callback。
        # rclpy 的 callback 都掛在 spin() 的同一個執行緒，不要做阻塞 I/O。
        # 注意：這裡傳的是 self.timer_callback（bound method 直接傳即可），
        # 不像 C++ 版要 std::bind——Python 物件方法天生就是 bound。
        self.timer = self.create_timer(0.5, self.timer_callback)

        # ROS 時鐘（不是 datetime.now()）——模擬器中會用模擬時間，與真實時間不同步
        self.start_time = self.get_clock().now()

    def timer_callback(self):
        msg = Twist()  # 預設值全 0，只填要覆蓋的欄位即可

        # rclpy.time.Time 的減法回傳 Duration，要從 nanoseconds 換算成秒
        elapsed = (self.get_clock().now() - self.start_time).nanoseconds / 1e9

        # 啟動後 3 秒前進，之後停下——刻意做「會自動結束」的行為
        if elapsed < 3.0:
            msg.linear.x = 0.2   # 0.2 m/s 前進
            msg.angular.z = 0.0  # angular.z 是 yaw 角速度
        else:
            msg.linear.x = 0.0
            msg.angular.z = 0.0

        # 真正把訊息送進 ROS 通訊層。沒有訂閱者時不會錯。
        self.publisher.publish(msg)


def main(args=None):
    # 初始化 rclpy 全域 context（DDS middleware、signal handler、--ros-args 解析）。
    # args=None 會從 sys.argv 自動撈，方便 ros2 run 時帶 --ros-args -r 等參數。
    rclpy.init(args=args)
    node = AutoDriveNode()
    try:
        # spin = 事件迴圈：阻塞主執行緒，持續處理 timer / subscription / service callback
        # 直到 SIGINT/SIGTERM。類比 mqtt_client.loop_forever()。
        rclpy.spin(node)
    except KeyboardInterrupt:
        # rclpy 處理 Ctrl+C 不像 rclcpp 那麼乾淨，要自己 catch 才不會印 traceback
        pass
    finally:
        # 順序很重要：先 destroy_node 釋放 publisher，再 shutdown 關 context
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
