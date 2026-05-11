# 👁️ Perception — 視覺感知

> 從 Phase 03 訂 LiDAR 擴到「**訂相機 → 偵測物件 → 處理 3D 點雲**」的 production-ready 視覺管線。

**狀態**:🟡 進行中 — 01 / 02 / 03 / 04 已寫文字草稿(⏸,等實際驗證)

---

## 🎯 學完整條支線你會

- 用 OpenCV + ROS 2 處理相機影像(`cv_bridge`)
- 用 AprilTag / ArUco 做機器人視覺定位(VSLAM 入門)
- 整合 YOLO 物件偵測,把結果發成自訂 `DetectedObjects.msg`
- 用 PCL(Point Cloud Library)處理 3D 點雲(濾波、分割、地面去除)
- 知道 `image_pipeline`、`vision_msgs`、`sensor_msgs/Image` 的標準用法

---

## 🏭 業界對應

**所有現代機器人公司都需要** — 不挑機器人類別:
- 移動底盤:看路標、避障、辨識物件
- 機械手臂:抓取點偵測、視覺伺服
- 無人機:visual odometry、目標追蹤
- 服務機器人:人臉 / 手勢辨識

**結論**:做 production 機器人沒有不會視覺的。比起任何單一機器人類別,**這條支線投資報酬率最高**。

---

## 📋 章節結構與進度

```
perception/
├── README.md                       ← 你正在讀
├── 01-camera-cv-bridge/            ⏸ 文字草稿(2026-05-05)
├── 02-apriltag-localization/       ⏸ 文字草稿(2026-05-05)
├── 03-yolo-ros2/                   ⏸ 文字草稿(已寫)
└── 04-pcl-pointcloud/              ⏸ 文字草稿(2026-05-05)
```

> **「⏸ 文字草稿」**:README + code 骨架已寫,但**沒在實機 / 雲端跑過驗證**。實做時可能需要修細節。雷區清單從業界經驗整理,實際跑可能會踩到沒寫的雷。

---

## 🧭 章節預告

### 01. Camera + cv_bridge — 訂閱影像

**學完你會**:
- 訂閱 `sensor_msgs/Image` topic(從 Gazebo 內建相機或實機)
- 用 `cv_bridge` 把 ROS Image 轉成 OpenCV `cv::Mat`
- 在 callback 裡做基本影像處理(灰階 / 邊緣偵測 / Canny)
- 發回 ROS Image topic 給 RViz 看
- 看穿「為什麼 Image 訊息那麼大,用 BestEffort QoS 才不丟包」這個雷

**核心套件**:
- `cv_bridge`(ROS ↔ OpenCV 橋接)
- `image_transport`(壓縮影像傳輸)
- Gazebo 的 `libgazebo_ros_camera.so` plugin(提供模擬相機)

**為什麼這章重要**:從 Phase 03 訂 LiDAR 自然擴展到訂 Image,是視覺管線的入門。
**預估時長**:1 day
**環境**:☁️/💻

---

### 02. AprilTag — 視覺定位

**學完你會**:
- 啟動 `apriltag_ros` 偵測畫面內的 AprilTag
- 從 tag 的相對位姿反推「機器人在世界的位置」
- 發 TF `world → base_link`(對比 Phase 16 的靜態 TF)
- 跟 robot_localization EKF(Phase 20A)整合,做視覺輔助定位

**為什麼重要**:
- AprilTag 是工業界**最簡單可靠的視覺定位方法**(全 MIT 授權)
- 用在倉儲機器人、停車場、工廠 AGV 標記點
- 比 SLAM 簡單一萬倍,但**精度極高**(< 1cm)

**核心套件**:`apriltag_ros`、`apriltag_msgs`

**整合主線**:
- Phase 16 TF2(發布 world → base_link)
- Phase 20A EKF(視覺 + 輪式 odometry 融合)

**預估時長**:1 day
**環境**:☁️ Gazebo 加 AprilTag SDF / 💻

---

### 03. YOLO + ROS 2 — 物件偵測

**學完你會**:
- 整合 YOLOv8 / YOLOv11 + ROS 2(用 `ultralytics` Python 套件)
- 把每幀偵測結果發成自訂 `DetectedObjects.msg`(配 [Phase 08 整合情境 2](../../INTERFACE_SELECTION.md#-情境-2協作手臂取放方塊manipulation))
- 視覺化:在 Image 上畫 bounding box 發回 RViz
- 訓練自己的 YOLO 模型 → 部署成 ROS Node(可選進階)

**核心套件**:
- `ultralytics`(YOLO Python lib)
- `vision_msgs`(ROS 2 標準的 Detection2DArray、BoundingBox2D)
- `image_pipeline`

**為什麼重要**:**YOLO + ROS 是業界深度學習機器人的事實組合**。Isaac ROS / Foxglove demo 都用這套。

**預估時長**:2 day(含模型下載 / GPU 設定)
**環境**:💻 本機(GPU 強烈推薦,雲端 ROSject GPU 受限)

---

### 04. PCL — 3D 點雲處理

**學完你會**:
- 訂閱 `sensor_msgs/PointCloud2`(從 Phase 03 LiDAR 自然延伸)
- 用 PCL 做 voxel grid 降採樣(原始 100k 點 → 10k 點)
- 平面分割(地面去除)— Nav2 預處理常用
- 物件分群(Euclidean clustering)抓出獨立物件
- 發出 `vision_msgs/Detection3DArray`

**核心套件**:`pcl_ros`、`pcl_conversions`、PCL(C++ 函式庫)

**為什麼重要**:
- 3D 點雲是**自駕車 / 倉儲機器人 / 高精度 SLAM** 標配
- 知道怎麼處理 PointCloud2 = 能進感知團隊

**預估時長**:2 day
**環境**:☁️/💻

---

## 📦 環境需求(本地)

```bash
# 主線 ROS 2 環境已就緒,額外裝:

# OpenCV + cv_bridge
sudo apt install ros-humble-cv-bridge ros-humble-image-transport \
                 ros-humble-image-transport-plugins

# AprilTag
sudo apt install ros-humble-apriltag ros-humble-apriltag-ros

# YOLO(Python)
pip install ultralytics

# PCL
sudo apt install ros-humble-pcl-ros ros-humble-pcl-conversions libpcl-dev

# vision_msgs
sudo apt install ros-humble-vision-msgs
```

---

## 🐛 預期會踩的雷(寫章節時逐條驗證)

1. **`cv_bridge` import failed: undefined symbol** — Python 跟 C++ cv_bridge 版本錯配,純 Python rclpy 必須用 `pip install opencv-python` 而非 `python3-opencv`
2. **Image 訊息丟一半** — 沒設 BestEffort QoS,reliable 對大訊息不友善
3. **AprilTag 偵測不到 tag** — 相機 calibration 沒做,fx/fy/cx/cy 全 0
4. **YOLO inference 慢(2 fps)** — 沒裝 GPU 版 PyTorch,跑 CPU 推論
5. **PCL `pcl::fromROSMsg` 卡住** — `sensor_msgs/PointCloud2` 的 `is_dense` 欄位沒設,PCL 拒絕

---

## 🔗 學習資源

- [ROS 2 vision_msgs 官方](https://github.com/ros-perception/vision_msgs)
- [Isaac ROS](https://github.com/NVIDIA-ISAAC-ROS) — NVIDIA 的 ROS 2 視覺套件(GPU 加速版)
- [PCL Tutorials](https://pcl.readthedocs.io/projects/tutorials/en/master/)
- [apriltag_ros 範例](https://github.com/christianrauch/apriltag_ros)

---

## 🚦 開始之前

確認主線進度(至少要做完):
- ✅ Phase 03(Subscriber + QoS)
- ✅ Phase 08(Custom Interfaces)— 第 03 章發 DetectedObjects 必備
- ✅ Phase 16(TF2)— 第 02 章 AprilTag 發 TF 必備
- 推薦 ✅ Phase 17(Gazebo)— 雲端 / 本機跑模擬相機

---

## ⏭️ 從哪開始

主線完成後,**跑 [01-camera-cv-bridge](01-camera-cv-bridge/)**(待寫)。
建議照 01 → 02 → 03 → 04 順序,後面章節依賴前面的觀念。

> 這條支線目前只有 README 骨架。實際章節會在 gino 開始做時逐章補。
