# 03. YOLO + ROS 2 — 物件偵測

> 將當今工業界最強的物件偵測模型 YOLO (YOLOv8 / YOLOv11) 整合進 ROS 2，並將辨識結果轉換為標準的 `vision_msgs` 或自訂訊息，這是現代 AI 機器人的標準配備。

**學完你會**:
- 整合 YOLO 與 ROS 2 (使用 `ultralytics` Python 套件)
- 訂閱 `sensor_msgs/Image`，執行 YOLO 推論，並將結果標註在影像上發回 RViz
- 將 Bounding Box 轉換為 ROS 標準訊息 (`vision_msgs/Detection2DArray`) 或我們在 Phase 08 定義的 `DetectedObjects`
- 了解 GPU 加速在 ROS 2 視覺管線中的重要性與配置方式
- 避免 Python 環境中 OpenCV 與 ROS 2 依賴衝突的經典大雷

**前置**:
- [Phase 01 Camera + cv_bridge](../01-camera-cv-bridge/) — 影像傳遞基礎
- [Phase 08 Custom Interfaces](../../../phase-08-custom-interfaces/) — 自訂訊息結構

**產出**:
- [`code/yolo_ros2_demo/`](code/yolo_ros2_demo/) — 執行 YOLO 推論並發布標註影像與偵測結果的 Python Node

**環境**:💻 本機 WSL2 / Ubuntu (強烈建議具備 NVIDIA GPU 加速)
> ☁️ TheConstructSim 雲端 ROSject 沒有配置強大的 GPU 給免費用戶，跑 YOLO 推論會非常緩慢 (大約 2-5 FPS)，但仍然可以用來驗證程式邏輯。

---

## 📍 為什麼 YOLO + ROS 2 是業界標配

傳統的電腦視覺 (如 Phase 01 的邊緣偵測、顏色追蹤) 對於光線與環境的容忍度極低。在現代的 AGV (無人搬運車) 或協作手臂上，無論是「尋找特定棧板」、「偵測前方行人」還是「辨識瑕疵品」，幾乎一律採用深度學習模型。

**YOLO (You Only Look Once)** 系列憑藉其極高的推論速度與良好的準確率，成為邊緣運算設備 (NVIDIA Jetson 系列) 上的絕對主流。將 `ultralytics` 套件包裝成 ROS 2 Node，是感知工程師最基礎也最常用的起手式。

---

## 💻 步驟 1: 建立 YOLO 推論 Node (Python)

我們使用 Python 來寫這個 Node，因為 AI 生態系對 Python 最友善。

完整程式碼見 [`code/yolo_ros2_demo/yolo_ros2_demo/yolo_detector.py`](code/yolo_ros2_demo/yolo_ros2_demo/yolo_detector.py)。

```python
import rclpy
from rclpy.node import Node
from sensor_msgs.msg import Image
from cv_bridge import CvBridge
import cv2
from ultralytics import YOLO

# 如果要發布標準訊息，可以 import vision_msgs
# from vision_msgs.msg import Detection2DArray, Detection2D, BoundingBox2D

class YoloDetector(Node):
    def __init__(self):
        super().__init__('yolo_detector')
        
        # 載入 YOLO 模型 (初次執行會自動下載 yolov8n.pt)
        self.get_logger().info("Loading YOLO model...")
        self.model = YOLO('yolov8n.pt')
        self.get_logger().info("Model loaded!")

        self.bridge = CvBridge()

        # 訂閱相機影像 (注意使用 SensorDataQoS 以避免延遲)
        from rclpy.qos import qos_profile_sensor_data
        self.sub_image = self.create_subscription(
            Image,
            'image_in',
            self.image_callback,
            qos_profile_sensor_data
        )

        # 發布帶有 Bounding Box 的影像
        self.pub_image = self.create_publisher(Image, 'image_out', qos_profile_sensor_data)
        
        # TODO: 可以額外發布 vision_msgs/Detection2DArray 給其他 Node 使用

    def image_callback(self, msg: Image):
        # 1. ROS Image -> cv2 影像
        try:
            cv_image = self.bridge.imgmsg_to_cv2(msg, desired_encoding='bgr8')
        except Exception as e:
            self.get_logger().error(f"cv_bridge exception: {e}")
            return

        # 2. YOLO 推論
        # conf=0.5 表示信心度低於 50% 的物件不顯示
        results = self.model(cv_image, conf=0.5, verbose=False)

        # 3. 繪製 Bounding Box (ultralytics 提供 plot() 方法自動畫圖)
        if len(results) > 0:
            annotated_frame = results[0].plot()
        else:
            annotated_frame = cv_image

        # 4. cv2 影像 -> ROS Image 並發布
        out_msg = self.bridge.cv2_to_imgmsg(annotated_frame, encoding='bgr8')
        out_msg.header = msg.header # 保持原有的時間戳與 frame_id
        self.pub_image.publish(out_msg)

def main(args=None):
    rclpy.init(args=args)
    node = YoloDetector()
    rclpy.spin(node)
    node.destroy_node()
    rclpy.shutdown()

if __name__ == '__main__':
    main()
```

---

## ⚙️ 步驟 2: package.xml 與 setup.py

在 `package.xml` 中加入依賴：
```xml
<depend>rclpy</depend>
<depend>sensor_msgs</depend>
<depend>cv_bridge</depend>
<!-- <depend>vision_msgs</depend> -->
```

在 `setup.py` 中註冊 entry point：
```python
entry_points={
    'console_scripts': [
        'yolo_detector = yolo_ros2_demo.yolo_detector:main',
    ],
},
```

---

## 🚀 步驟 3: 跑 Demo

### 💻 本機安裝依賴與編譯

因為用到 `ultralytics`，我們需要用 pip 安裝：

```bash
# 安裝 Ultralytics
pip install ultralytics

# 如果你的 cv_bridge 在 Python 中會報錯，通常是因為 ROS 內建的 OpenCV 和 pip 裝的打架了
# 業界常見解法：確保安裝 opencv-python
pip install opencv-python
```

編譯並執行：

```bash
cd ~/ros2_ws
colcon build --packages-select yolo_ros2_demo
source install/setup.bash
```

啟動模擬相機環境（例如 Turtlebot3 Waffle 或接上你的 USB Webcam 發布 image_raw）：

```bash
# 終端機 1：啟動 Turtlebot3 Waffle (帶相機)
export TURTLEBOT3_MODEL=waffle
ros2 launch turtlebot3_gazebo turtlebot3_world.launch.py

# 終端機 2：啟動 YOLO Node
ros2 run yolo_ros2_demo yolo_detector --ros-args -r image_in:=/camera/image_raw -r image_out:=/yolo/annotated_image
```

### 🎯 驗證效果

打開 RViz2 或 Foxglove，訂閱 `/yolo/annotated_image`。當 Turtlebot 在 Gazebo 中四處移動時，你應該會看到畫面上出現 YOLO 畫好的 Bounding Box (例如辨識出 `chair`, `potted plant` 等)。

如果你的機器有 NVIDIA GPU 並且 PyTorch 配置正確，推論速度應該能跟上相機的 30 FPS。如果使用 CPU 推論，可能會感覺到明顯的卡頓。

---

## 🐛 常見雷

### ⚠️ 雷 1：`cv_bridge` 報錯 `ImportError: libopencv_core.so.4.2: cannot open shared object file`

**症狀**：Python 腳本在 import cv_bridge 時直接崩潰。
**原因**：ROS 2 系統層級的 OpenCV 版本與你用 `pip install opencv-python` 或其他 AI 套件帶進來的 OpenCV 版本產生了衝突（ABI 不相容）。
**解**：這是一個世紀大雷。通常的解法是移除 `pip` 裝的 opencv，或者在一個乾淨的 Python 虛擬環境 (Virtual Environment) 中重新編譯 `cv_bridge`。在 Humble 搭配 Ubuntu 22.04 下，儘量避免用 `pip` 安裝 opencv，而是使用 `sudo apt install python3-opencv`。

### ⚠️ 雷 2：YOLO 瘋狂印出推論日誌，淹沒了 ROS Log

**症狀**：終端機被 `0: 480x640 1 person, 2 cars, 12.3ms` 洗版。
**原因**：Ultralytics 預設會把每一次推論的結果 print 出來。
**解**：在呼叫 `model()` 時加入 `verbose=False` 參數：
`results = self.model(cv_image, verbose=False)`。

### ⚠️ 雷 3：推論速度太慢，導致延遲越來越大

**症狀**：雖然用了 `SensorDataQoS`，但畫面延遲還是越來越嚴重，過了 10 秒才看到影像的變動。
**原因**：如果推論一幀需要 100ms (10 FPS)，但相機以 30 FPS 發送，Node 的 Callback Queue 可能會積壓。
**解**：
1. 確保使用了 `SensorDataQoS` (它的 History 深度預設只有 5，甚至可以手動設為 1)。
2. 在程式內手動丟棄舊的幀：紀錄上一幀的處理時間，如果間隔太短直接 `return`。
3. 確保 `ultralytics` 抓到了 GPU (`device='cuda'`)。

### ⚠️ 雷 4：想要在 RViz 裡顯示標準的 3D Box 而不是 2D 圖片

**症狀**：目前只發布了 2D 圖片，導航系統無法得知物件的三維位置。
**原因**：YOLO 預設是 2D 物件偵測。
**解**：這需要進階技巧。你需要結合深度相機 (Depth Camera) 的點雲資料。將 YOLO 產生的 2D Bounding Box 對應到 Depth Map 上取得 Z 軸距離，然後發布 `vision_msgs/Detection3DArray` 甚至轉換為 TF 框架。

---

## 🎯 學到的關鍵概念

- **YOLO 整合的極簡化**：受惠於 `ultralytics`，現在在 ROS 2 中跑最先進的 AI 模型只需要不到 10 行程式碼。
- **Python 環境衝突**：深刻體會到 ROS 2 的系統級依賴與 Python pip 生態系的衝突痛點。
- **QoS 的必要性**：影像處理極度依賴 `qos_profile_sensor_data` 以避免網路與記憶體被塞爆。

---

## 🌟 進階挑戰

1. **發布語意資料**：不只發布影像，解析 `results[0].boxes`，並發布 `vision_msgs/Detection2DArray` 給其他 Node（例如讓手臂去抓取特定物件）。
2. **訓練客製化模型**：去 Roboflow 標註你自己的資料集（例如特定的螺絲、工具），訓練出 `best.pt` 並替換掉預設的 `yolov8n.pt`。
3. **TensorRT 加速**：將 `.pt` 模型轉換為 `.engine` 格式，在 NVIDIA Jetson 設備上將推論速度提升 3 倍。

---

## ⏸ 驗證前 audit checklist (留給跑驗證的人)

- [ ] **WSL2 / 雲端環境中 `ultralytics` 安裝是否順利**：確認是否需要特定的 PyTorch 版本以配合 CUDA。
- [ ] **cv_bridge 衝突測試**：在 Ubuntu 22.04 + ROS 2 Humble 下，安裝 `ultralytics` 是否會弄壞 `cv_bridge`。
- [ ] **Turtlebot3 的相機 Topic 名稱**：確認 `turtlebot3_world` 啟動後，waffle 模型的確切影像 Topic 是不是 `/camera/image_raw`。
