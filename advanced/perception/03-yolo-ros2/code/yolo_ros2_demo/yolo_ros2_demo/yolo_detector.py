import rclpy
from rclpy.node import Node
from sensor_msgs.msg import Image
from cv_bridge import CvBridge
import cv2
from ultralytics import YOLO

class YoloDetector(Node):
    def __init__(self):
        super().__init__('yolo_detector')
        
        self.get_logger().info("Loading YOLO model...")
        # 載入輕量級模型 yolov8n.pt，若本地沒有會自動下載
        self.model = YOLO('yolov8n.pt')
        self.get_logger().info("Model loaded!")

        self.bridge = CvBridge()

        # 訂閱相機影像，使用 SensorDataQoS 以減少延遲與記憶體佔用
        from rclpy.qos import qos_profile_sensor_data
        self.sub_image = self.create_subscription(
            Image,
            'image_in',
            self.image_callback,
            qos_profile_sensor_data
        )

        # 發布畫好 Bounding Box 的影像
        self.pub_image = self.create_publisher(Image, 'image_out', qos_profile_sensor_data)

    def image_callback(self, msg: Image):
        # 1. 將 ROS Image 轉換為 OpenCV 影像 (cv::Mat)
        try:
            cv_image = self.bridge.imgmsg_to_cv2(msg, desired_encoding='bgr8')
        except Exception as e:
            self.get_logger().error(f"cv_bridge exception: {e}")
            return

        # 2. 進行 YOLO 推論
        # conf=0.5 確保只顯示信心度 >= 50% 的物件
        # verbose=False 避免推論結果洗版 Console
        results = self.model(cv_image, conf=0.5, verbose=False)

        # 3. 在影像上繪製 Bounding Box
        if len(results) > 0:
            # results[0].plot() 會回傳畫好框框的 numpy array (OpenCV image)
            annotated_frame = results[0].plot()
        else:
            annotated_frame = cv_image

        # 4. 將 OpenCV 影像轉換回 ROS Image
        try:
            out_msg = self.bridge.cv2_to_imgmsg(annotated_frame, encoding='bgr8')
            # 保持原始的時間戳與 Frame ID，這對後續的 TF 座標轉換非常重要
            out_msg.header = msg.header
            self.pub_image.publish(out_msg)
        except Exception as e:
            self.get_logger().error(f"Failed to publish image: {e}")

def main(args=None):
    rclpy.init(args=args)
    node = YoloDetector()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()

if __name__ == '__main__':
    main()
