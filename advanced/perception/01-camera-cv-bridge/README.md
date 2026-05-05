# 01. Camera + cv_bridge — 訂閱影像並用 OpenCV 處理

> 從 Phase 03 訂 LiDAR 自然延伸到「**訂相機 + 用 OpenCV 處理 + 發回處理過的 Image**」。Production 視覺管線的入門關卡。

**學完你會**:
- 訂閱 `sensor_msgs/Image`(從 Gazebo 內建相機,或實機 webcam)
- 用 `cv_bridge` 把 ROS Image 轉成 OpenCV `cv::Mat`
- 在 callback 內做基本影像處理(灰階、Canny edge detection)
- 發回另一條 Image topic 給 RViz / Foxglove 看
- 看穿 Image 訊息對 QoS 的特殊需求(BestEffort 才不丟包)

**前置**:
- [Phase 03 Subscriber + 光達避障](../../../phase-03-subscriber-lidar-brake/) — Sub + QoS 觀念
- [Phase 17 Gazebo](../../../phase-17-gazebo/) — Gazebo 怎麼附帶相機

**產出**:[`code/my_camera_demo/`](code/my_camera_demo/) — 訂 Image + OpenCV 處理 + 發回的 C++ Node

**環境**:☁️ TheConstructSim(內建 turtlebot3 + 相機)/ 💻 本機 WSL2(可用 turtlebot3_world)

---

## 📍 為什麼影像處理要單獨一章

影像跟 LiDAR 一樣是 sensor 資料,但**訊息特性差很大**:

| 維度 | LiDAR(PointCloud2) | 相機(Image) |
|------|---------------------|--------------|
| 資料量 | ~1 MB/scan | ~6 MB/frame(1920×1080 RGB) |
| 頻率 | 5–10 Hz | 30 Hz |
| 處理函式庫 | PCL | **OpenCV** |
| ROS 整合層 | 直接 sensor_msgs | 需要 **cv_bridge** 橋接 |
| QoS 推薦 | SensorDataQoS | SensorDataQoS(BestEffort) |

**核心橋接**:`cv_bridge` 把 `sensor_msgs/Image` 跟 `cv::Mat` 互轉。沒這層你只能拿到一個 byte array,沒辦法用 OpenCV 函式。

---

## ⚠️ 關鍵知識:Image 訊息的 QoS 為什麼特別重要

Image 一張 1920×1080 RGB = 6 MB,30 Hz = **180 MB/s**。

預設 ROS QoS(`Reliable + Volatile`):
- `Reliable` 會重傳掉的封包 → 6 MB 訊息只要丟一個 packet 就要整個重送 → CPU/網路爆
- 訂閱者跟不上時,**publisher 會 block 整個 callback**

正確做法:**`SensorDataQoS`(內含 BestEffort + KeepLast 5)**:
- 丟掉的封包不重傳(影像 30 Hz,丟 1 幀沒事下一幀會到)
- KeepLast 5:訂閱者只看最新 5 幀,舊的丟掉
- publisher 不 block

```cpp
// ❌ 預設 QoS,Image 訊息會卡
auto sub = create_subscription<sensor_msgs::msg::Image>(
    "image_raw", 10, callback);

// ✅ SensorDataQoS
auto sub = create_subscription<sensor_msgs::msg::Image>(
    "image_raw", rclcpp::SensorDataQoS(), callback);
```

---

## 💻 步驟 1:寫 Image 處理 Node

完整見 [`code/my_camera_demo/src/edge_detector.cpp`](code/my_camera_demo/src/edge_detector.cpp)。

```cpp
#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/image.hpp>
#include <cv_bridge/cv_bridge.h>
#include <opencv2/opencv.hpp>

class EdgeDetector : public rclcpp::Node
{
public:
    EdgeDetector() : Node("edge_detector")
    {
        // 訂閱 — SensorDataQoS 必須
        sub_ = create_subscription<sensor_msgs::msg::Image>(
            "image_in", rclcpp::SensorDataQoS(),
            std::bind(&EdgeDetector::on_image, this, std::placeholders::_1));

        // 發回處理過的 image
        pub_ = create_publisher<sensor_msgs::msg::Image>(
            "image_out", rclcpp::SensorDataQoS());

        RCLCPP_INFO(get_logger(), "edge_detector ready");
    }

private:
    void on_image(const sensor_msgs::msg::Image::SharedPtr msg)
    {
        // 1. ROS Image → cv::Mat
        cv_bridge::CvImagePtr cv_ptr;
        try {
            cv_ptr = cv_bridge::toCvCopy(msg, "bgr8");
        } catch (cv_bridge::Exception & e) {
            RCLCPP_ERROR(get_logger(), "cv_bridge error: %s", e.what());
            return;
        }

        cv::Mat & img = cv_ptr->image;

        // 2. 處理 — 灰階 + Canny edge
        cv::Mat gray, edges;
        cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);
        cv::Canny(gray, edges, 100, 200);

        // 3. cv::Mat → ROS Image,維持 header(時間戳 + frame_id)
        auto out = cv_bridge::CvImage(
            msg->header,         // 重要:用原 header 才能跟 TF 對齊
            "mono8",
            edges).toImageMsg();

        pub_->publish(*out);
    }

    rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr sub_;
    rclcpp::Publisher<sensor_msgs::msg::Image>::SharedPtr pub_;
};

int main(int argc, char * argv[])
{
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<EdgeDetector>());
    rclcpp::shutdown();
    return 0;
}
```

**重點**:
- `cv_bridge::toCvCopy` 會 deep copy → 安全可改;`toCvShared` 是 zero-copy 但 read-only
- 第二參數 `"bgr8"` 是目標 encoding,**OpenCV 慣用 BGR 不是 RGB**(歷史遺留)
- 輸出時用**原 header**(`msg->header`),保留時間戳跟 frame_id,下游做 visual servoing 才能跟 TF 對齊

---

## ⚙️ 步驟 2:CMakeLists.txt 雷區

完整見 [`code/my_camera_demo/CMakeLists.txt`](code/my_camera_demo/CMakeLists.txt)。

```cmake
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(cv_bridge REQUIRED)
find_package(OpenCV REQUIRED)         # ← 注意:不是 ament_target,用 target_link

add_executable(edge_detector src/edge_detector.cpp)
ament_target_dependencies(edge_detector
    rclcpp sensor_msgs cv_bridge)

# OpenCV 不是 ament 套件,要用 target_link_libraries 接
target_link_libraries(edge_detector ${OpenCV_LIBS})
```

> ⚠️ **新手雷**:OpenCV 是 system-level 套件不是 ament,**不能放在 `ament_target_dependencies`**。要 `target_link_libraries(... ${OpenCV_LIBS})`。

---

## 🚀 步驟 3:跑 Demo

### ☁️ TheConstructSim 步驟

雲端 turtlebot3_world ROSject 已內建相機。

```bash
# 1. clone
cd ~/ros2_ws/src
git clone https://github.com/gino07172002/ros2-learning-notes.git
cp -r ros2-learning-notes/advanced/perception/01-camera-cv-bridge/code/my_camera_demo .

# 2. Build
cd ~/ros2_ws
colcon build --packages-select my_camera_demo
source install/setup.bash

# 3. 啟動 turtlebot3_world(內含相機)
export TURTLEBOT3_MODEL=waffle    # waffle 才有相機,burger 沒有
ros2 launch turtlebot3_gazebo turtlebot3_world.launch.py

# 4. 另開 terminal 跑 edge_detector,把 turtlebot 相機 topic remap 進來
ros2 run my_camera_demo edge_detector \
  --ros-args -r image_in:=/camera/image_raw -r image_out:=/camera/edges
```

### 💻 本機 WSL2 步驟

跟雲端一樣,但 turtlebot3_world 在 WSL2 GPU 不足會慢。建議用 `headless:=true` 跑:

```bash
ros2 launch turtlebot3_gazebo turtlebot3_world.launch.py headless:=true
```

---

## 🎯 步驟 4:驗證 + 看效果

```bash
# 1. 確認 topics 在
ros2 topic list | grep camera
# 預期:
#   /camera/image_raw
#   /camera/edges            ← 我們發的

# 2. 看 image_in 的型別 + Hz
ros2 topic hz /camera/image_raw
# 預期:30 Hz 上下

ros2 topic info /camera/image_raw
# 預期:Type: sensor_msgs/msg/Image
#       QoS: Reliability=BEST_EFFORT(turtlebot3 預設用 SensorDataQoS)

# 3. 看處理後的 edges
ros2 topic hz /camera/edges
# 預期:跟 input 同 30 Hz(callback 跟得上)
```

### 視覺化:用 Foxglove 看

最簡單的方式 — 跑 Phase 35 的 foxglove_bridge:

```bash
ros2 run foxglove_bridge foxglove_bridge
# 瀏覽器:https://app.foxglove.dev → ws://localhost:8765
# 加 Image panel 訂 /camera/image_raw 跟 /camera/edges 對比
```

預期看到:
- `/camera/image_raw`:turtlebot 看到的 Gazebo 場景
- `/camera/edges`:同畫面但只剩白色 edge 線條(Canny 輸出)

---

## 🐛 常見雷

### 雷 1:`cv_bridge.h` include 找不到

**症狀**:`fatal error: cv_bridge/cv_bridge.h: No such file or directory`

**原因**:沒裝 `ros-humble-cv-bridge` 或 CMake 沒 `find_package(cv_bridge REQUIRED)`。

**解**:
```bash
sudo apt install ros-humble-cv-bridge ros-humble-vision-opencv
```
然後 CMake 補 `find_package(cv_bridge REQUIRED)` + `ament_target_dependencies(... cv_bridge)`。

### 雷 2:OpenCV 也找不到

**症狀**:`fatal error: opencv2/opencv.hpp: No such file`

**原因**:Ubuntu 22.04 / ROS Humble 預設只裝 OpenCV 4.5 的 cv_bridge wrapper,**OpenCV header 沒裝**。

**解**:
```bash
sudo apt install libopencv-dev
```

### 雷 3:訊息型別錯誤 — `cannot convert msg encoding`

**症狀**:runtime 噴
```
[ERROR] cv_bridge.cpp: Image is wrong size or type. Expected mono8, got rgb8
```

**原因**:`toCvCopy(msg, "bgr8")` 強制轉 BGR,但有時 Gazebo 相機用 `rgb8` 編碼,encoding 不一致 cv_bridge 會抱怨。

**解**:用 `cv_bridge::toCvCopy(msg, sensor_msgs::image_encodings::BGR8)` 讓 cv_bridge 自己處理 RGB↔BGR 轉換,不要強寫字串。

### 雷 4:Image 訊息丟一半 / Hz 比 publisher 慢

**症狀**:`/camera/image_raw` 30 Hz,但 `/camera/edges` 只 15 Hz。

**原因**:**訂閱用了預設 Reliable QoS,turtlebot publisher 用 BestEffort,QoS 不兼容**;或 callback 處理慢於 30 Hz frame。

**解**:
1. Subscribe 改 `rclcpp::SensorDataQoS()`(本範例已用)
2. 處理慢的話降 Canny 解析度(先 cv::resize 再 Canny)

### 雷 5:`bgr8` vs `rgb8` 顏色看起來怪怪的

**症狀**:RViz / Foxglove 看影像,**藍紅顛倒**。

**原因**:OpenCV 用 BGR 但 RViz 預設是 RGB。當你 `toCvCopy(msg, "bgr8")` 然後 publish 出去,header 還是 `bgr8`,RViz 解釋成 BGR 顯示對。但若你發 `rgb8` encoding 的訊息卻內容是 BGR,顏色會反。

**解**:輸出時 encoding 跟 cv::Mat 內容一致就好(本範例 mono8 沒這問題)。

### 雷 6:雲端 ROSject 內 turtlebot3 burger 沒相機

**症狀**:`TURTLEBOT3_MODEL=burger`,`ros2 topic list` 沒看到 `/camera/image_raw`。

**原因**:turtlebot3 的 **burger 沒帶相機**,只有 LiDAR;**waffle 才有 RGB 相機 + Lidar**。

**解**:`export TURTLEBOT3_MODEL=waffle` 再 launch。

---

## 🎯 學到的關鍵概念

- **cv_bridge** 是 ROS Image ↔ OpenCV cv::Mat 的橋接層
- **SensorDataQoS(BestEffort)** 是 Image 訊息標配,Reliable 對大訊息會卡
- **OpenCV 慣用 BGR** 不是 RGB(歷史遺留),encoding 字串要對齊
- 輸出 Image 時**保留原 header**(時間戳 + frame_id),下游 TF 對齊靠這個
- CMake 處理 OpenCV 用 `target_link_libraries` 不是 ament_target_dependencies

---

## 🌟 進階挑戰

1. **加 ROS 2 Image 壓縮傳輸**:用 `image_transport` 改用 `compressed` plugin,頻寬降 10 倍
2. **加 GUI 即時看**:用 `cv::imshow` 直接彈出視窗(WSLg / 雲端 X11 都支援)
3. **加 timing benchmark**:在 callback 內 `auto t = now()` 算每幀 Canny 處理時間
4. **接 Phase 37 LifecycleNode**:把 EdgeDetector 包成 LifecycleNode,可以 runtime 切 enable/disable

---

## 下一步

- [02. AprilTag 視覺定位](../02-apriltag-localization/)(待寫) — 用 AprilTag 反推機器人位置,發 TF
- [04. PCL 點雲處理](../04-pcl-pointcloud/)(待寫) — 從 LiDAR 換到 3D 點雲處理

---

---

## ⏸ 驗證前 audit checklist(留給跑驗證的人)

從 [`verify_log.md`](../../../verify_log.md) 學到:文字草稿就算「同模式」也常 build 失敗。跑 demo 前先看:

- [ ] **`cv_bridge` 在 Humble 的 include path**:Humble 的 `cv_bridge.h` 在 `cv_bridge/cv_bridge.hpp`(C++ 新版)還是 `cv_bridge/cv_bridge.h`(舊),不同版本文件混雜 — 我寫 `.h` 可能要改 `.hpp`
- [ ] **`sensor_msgs::image_encodings::BGR8`**:這個常數的 namespace 在某些版本是 `sensor_msgs::image_encodings::BGR8`,有些版本要 `#include <sensor_msgs/image_encodings.hpp>` 才有。沒 include 會編譯錯
- [ ] **CMakeLists `target_link_libraries(... ${OpenCV_LIBS})`**:確認 `find_package(OpenCV REQUIRED)` 之後 OpenCV_LIBS 變數真的有東西(某些 distro 變成 `OpenCV::OpenCV` target)
- [ ] **turtlebot3 waffle 真的有相機**:雲端 / 本機跑 `TURTLEBOT3_MODEL=waffle` + `ros2 topic list` 看是否有 `/camera/image_raw`(burger 沒,雷 6)
- [ ] **edge_detector 跑起來 CPU 是否吃不消**:Canny 在 CPU 跑 30 Hz × 1280x720 可能滿載,需要降解析度或限頻
- [ ] **publish 的 image_out frame_id 是否保留**:dump `ros2 topic echo /camera/edges --field header --once` 確認 frame_id 不是空字串

跑通後升 ✅,把實際踩到的雷補進「常見雷」段。

---

> **驗證狀態**:⏸ 純文字草稿(2026-05-05) — code 結構照 Phase 03 + cv_bridge 官方範例整理。雷 1–6 從業界經驗。雲端 / WSL 實際驗證後升級成 ✅。
