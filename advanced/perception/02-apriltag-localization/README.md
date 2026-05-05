# 02. AprilTag — 視覺定位

> 用 AprilTag(MIT 開源的 fiducial marker)在相機畫面內定位機器人。**比 SLAM 簡單一萬倍,但精度極高(< 1cm)**。業界倉儲機器人 / 工廠 AGV 用一堆。

**學完你會**:
- 用 `apriltag_ros` Node 偵測畫面中的 tag,輸出 tag pose
- 從 tag 的相對位姿反推「機器人在世界的位置」
- 發 TF `world → base_link`(對比 Phase 16 靜態 TF + Phase 20A EKF 的動態 TF)
- 知道 AprilTag 跟 ArUco 的差異(用哪個)
- 完成相機內參 calibration(`fx`/`fy`/`cx`/`cy`)是 AprilTag 偵測前置必做

**前置**:
- [01. Camera + cv_bridge](../01-camera-cv-bridge/) — 訂 Image 觀念
- [Phase 16 TF2](../../../phase-16-tf2/) — 動態 TF broadcaster

**產出**:[`code/my_apriltag_demo/`](code/my_apriltag_demo/) — apriltag_ros launch + 視覺定位 Node

**環境**:☁️ TheConstructSim(內建相機 + 自加 AprilTag SDF) / 💻 本機 WSL2

---

## 📍 為什麼業界愛 AprilTag

跟 SLAM 比:

| 維度 | SLAM | AprilTag |
|------|------|---------|
| 設定難度 | 高(slam_toolbox 上百個參數) | 低(印幾張紙貼起來) |
| 精度 | 5–20 cm | < 1 cm |
| 適合場景 | 開放式、無結構環境 | 結構化環境(倉庫、工廠) |
| 計算成本 | GPU 強烈推薦 | CPU 純秒解 |
| 受光線影響 | 中 | 中(tag 對比度要夠) |

**結論**:**有「可控制環境」的應用全部用 AprilTag**。倉庫地面貼 tag、工廠每個工位貼 tag,機器人看一眼就知道自己在哪。SLAM 是「沒辦法時才用」的方案。

---

## ⚠️ 關鍵知識:相機內參(Intrinsics)是必須的

AprilTag 偵測流程:**畫面內找黑白方塊 → 解出 tag 的 3D 位姿**(相對相機)。要解 3D 位姿就需要知道相機的:
- **fx, fy** — 焦距(像素單位)
- **cx, cy** — 畫面中心點
- **distortion**(失真係數)

這些參數合起來叫**相機內參**(intrinsics)。每台相機不一樣,**必須先 calibration 一次**。

### Gazebo 模擬相機:內參自動生

Gazebo 內建相機的 `<camera_info>` 訊息**自動含正確內參**(根據 SDF 內 FOV / 解析度算出來)→ 模擬不用手動 calibration。

### 實機:必做相機 calibration

```bash
# ROS 2 標準 calibration 工具
sudo apt install ros-humble-camera-calibration
ros2 run camera_calibration cameracalibrator \
  --size 8x6 --square 0.025 \
  --ros-args -r image:=/your/camera/image_raw -r camera:=/your/camera

# 印一張 8x6 棋盤格(0.025m 一格),拿在相機前晃,直到 4 個 bar 都滿
# 按 CALIBRATE → SAVE,生成 ost.yaml
```

> ⚠️ **新手雷**:沒 calibration 直接跑 AprilTag,**tag pose 估計會差 30%+**。Gazebo 模擬不用,實機必做。

---

## 🎯 設計目標

```
Gazebo 場景:
  地面貼 4 張 AprilTag(world frame 內已知座標)
  Tag ID 0:位置 ( 1.0, 0.0, 0.0)
  Tag ID 1:位置 (-1.0, 0.0, 0.0)
  Tag ID 2:位置 ( 0.0, 1.0, 0.0)
  Tag ID 3:位置 ( 0.0,-1.0, 0.0)

機器人帶相機朝下:
  相機看到 Tag → apriltag_ros 算出 tag 在相機 frame 的位姿
  → 我們的 localizer 反算「機器人在 world frame 的位姿」
  → 發 TF world → base_link

預期結果:
  打開 RViz → Fixed Frame=world
  看到機器人位置隨 Gazebo 真實位置移動而更新
```

---

## 💻 步驟 1:設定 apriltag_ros

完整 config 見 [`code/my_apriltag_demo/config/apriltag.yaml`](code/my_apriltag_demo/config/apriltag.yaml)。

```yaml
# apriltag.yaml — apriltag_ros 設定
apriltag_node:
  ros__parameters:
    family: 36h11           # AprilTag 家族,36h11 是現代主流
    size: 0.16              # tag 黑邊框長度(公尺),要跟印出來的實際大小一致
    max_hamming: 0          # 容錯位元數,0 = 嚴格

    # 已知的 tag → 世界座標對應表
    tag:
      ids: [0, 1, 2, 3]
      frames: [tag_0, tag_1, tag_2, tag_3]
      sizes: [0.16, 0.16, 0.16, 0.16]
```

---

## 💻 步驟 2:Localizer Node — 從 tag pose 反推機器人位置

完整見 [`code/my_apriltag_demo/src/localizer.cpp`](code/my_apriltag_demo/src/localizer.cpp)。

```cpp
#include <rclcpp/rclcpp.hpp>
#include <apriltag_msgs/msg/april_tag_detection_array.hpp>
#include <tf2_ros/static_transform_broadcaster.h>
#include <tf2_ros/transform_broadcaster.h>
#include <tf2/LinearMath/Transform.h>
#include <tf2_geometry_msgs/tf2_geometry_msgs.hpp>

// 已知的 tag 在 world frame 的位置(實際應用會從 yaml 讀)
const std::map<int, std::array<double, 3>> TAG_POSES = {
    {0, { 1.0,  0.0, 0.0}},
    {1, {-1.0,  0.0, 0.0}},
    {2, { 0.0,  1.0, 0.0}},
    {3, { 0.0, -1.0, 0.0}},
};

class AprilTagLocalizer : public rclcpp::Node
{
public:
    AprilTagLocalizer() : Node("apriltag_localizer")
    {
        sub_ = create_subscription<apriltag_msgs::msg::AprilTagDetectionArray>(
            "tag_detections", 10,
            std::bind(&AprilTagLocalizer::on_detections, this, std::placeholders::_1));

        tf_broadcaster_ = std::make_shared<tf2_ros::TransformBroadcaster>(this);
    }

private:
    void on_detections(
        const apriltag_msgs::msg::AprilTagDetectionArray::SharedPtr msg)
    {
        for (const auto & det : msg->detections) {
            auto it = TAG_POSES.find(det.id);
            if (it == TAG_POSES.end()) continue;

            // det.pose 是 tag 在 camera frame 的位姿
            // 我們要反推 camera 在 world frame 的位姿(然後串到 base_link)

            // 簡化版:假設 tag 與 world 軸平行,只取平移
            const auto & world_pos = it->second;
            // tag 在 camera 的相對位置 (cam → tag) 反向得到 (tag → cam)
            double cam_x_in_world = world_pos[0] - det.pose.position.x;
            double cam_y_in_world = world_pos[1] - det.pose.position.y;
            double cam_z_in_world = world_pos[2] - det.pose.position.z;

            geometry_msgs::msg::TransformStamped t;
            t.header.stamp = msg->header.stamp;
            t.header.frame_id = "world";
            t.child_frame_id = "base_link";
            t.transform.translation.x = cam_x_in_world;
            t.transform.translation.y = cam_y_in_world;
            t.transform.translation.z = cam_z_in_world;
            t.transform.rotation.w = 1.0;   // 簡化:朝向不變

            tf_broadcaster_->sendTransform(t);

            // 用第一個偵測到的 tag 即可
            return;
        }
    }

    rclcpp::Subscription<apriltag_msgs::msg::AprilTagDetectionArray>::SharedPtr sub_;
    std::shared_ptr<tf2_ros::TransformBroadcaster> tf_broadcaster_;
};

int main(int argc, char * argv[])
{
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<AprilTagLocalizer>());
    rclcpp::shutdown();
    return 0;
}
```

> ⚠️ 上面的 code 是**簡化版**,只處理平移、忽略旋轉。實機要用 `tf2::Transform` 完整反算 4×4 matrix。

---

## 🚀 步驟 3:跑 Demo

### ☁️ TheConstructSim 步驟

```bash
sudo apt install ros-humble-apriltag-ros ros-humble-apriltag-msgs
# (雲端 ROSject 通常已預裝)

cd ~/ros2_ws/src
git clone https://github.com/gino07172002/ros2-learning-notes.git
cp -r ros2-learning-notes/advanced/perception/02-apriltag-localization/code/my_apriltag_demo .

cd ~/ros2_ws
colcon build --packages-select my_apriltag_demo
source install/setup.bash

ros2 launch my_apriltag_demo apriltag_demo.launch.py
```

### 💻 本機 WSL2

同上。要先確認 turtlebot3_world 內有 AprilTag SDF(本章 launch 會 spawn)。

---

## 🎯 步驟 4:驗證

```bash
# 1. 確認 apriltag_node 在發 detections
ros2 topic echo /tag_detections --once
# 預期看到:
#   detections:
#     - id: 0
#       pose: ...

# 2. 確認 TF tree 有 world → base_link
ros2 run tf2_ros tf2_echo world base_link
# 預期看到 translation 隨機器人位置更新

# 3. 用 Foxglove / RViz 看
# Fixed Frame=world,加 RobotModel + TF panel
```

---

## 🐛 常見雷

### 雷 1:`apriltag_node` 啟動但 detections 永遠空

**症狀**:topic 在,但 `detections` array 永遠 length=0。

**可能原因**:
1. **camera_info 沒發**:apriltag_ros 需要 camera intrinsics 才能解 pose,沒收到就直接跳過
2. **tag size 不對**:yaml 內 `size: 0.16` 跟實際 SDF 內 tag 大小不一致
3. **光線太暗 / 對比度不夠**:Gazebo 場景沒打燈

**解**:
1. `ros2 topic hz /camera/camera_info` 確認在發
2. 對 SDF 內 tag mesh 大小(visual collision)
3. SDF 加 `<light>` tag

### 雷 2:tag 偵測到了但 pose 一直跳

**症狀**:detection 在,但 `pose.position.x` 在小數點後第 2 位跳動 ±0.1m。

**原因**:
- 相機解析度太低(640×480 → tag 在畫面內只佔幾像素 → 邊緣偵測不穩)
- 或 tag 太小 / 距離太遠

**解**:
- Gazebo 相機解析度提到 1280×720(改 SDF `<width>` `<height>`)
- 或讓 tag 更大(`size: 0.30`)/ 距離更近

### 雷 3:family 跟 tag 對不上

**症狀**:`family: 36h11` 但你印出來的是 `tag25h9`,完全偵測不到。

**解**:確認 yaml 內 `family` 跟你印出來 / SDF 內 tag 圖案匹配。**現代主流是 `36h11`**,舊系統可能用 `25h9` / `16h5`。

### 雷 4:TF 衝突 — `world → base_link` 跟 odometry TF 撞

**症狀**:除了 apriltag_localizer 發 `world → base_link`,還有 `odom → base_link` 從 robot_state_publisher 來,**RViz 顯示位置跳來跳去**。

**原因**:**ROS 標準 TF tree 是樹狀**,`base_link` 只能有一個父節點。同時兩個 broadcaster 發到同一個 child 會 race。

**解**(業界標準):
- AprilTag localizer 應該發 `world → odom`(校正 odom 漂移),不是直接到 `base_link`
- TF tree 變成:`world → odom → base_link`,各層分工

### 雷 5:AprilTag 模型在 Gazebo 內看不見

**症狀**:Gazebo 視窗打開,看不到 tag。

**原因**:tag 可能 spawn 到地下 / 太小。

**解**:確認 SDF 內 tag 的 `<pose>` z 軸 > 0,大小用 visual debug:

```bash
# 列出 Gazebo 內所有 model
ros2 service call /get_model_list ...
```

或乾脆 Gazebo viewer 內 `World → Models`,確認 tag 模型有在 list 上。

---

## 🎯 學到的關鍵概念

- AprilTag 是「**有結構環境的便宜定位方案**」,精度比 SLAM 高、設定難度低
- **相機 calibration**(內參)是 AprilTag 必做,實機不能跳過(模擬有自動帶)
- TF tree 設計:**AprilTag 校正應該發 `world → odom`,不是直接 `world → base_link`**(避免跟 odometry TF 撞)
- `family: 36h11` 是現代主流,舊系統才用 `25h9`/`16h5`
- 模擬看 tag pose 不穩 → 加大相機解析度 + 拉近 tag

---

## 🌟 進階挑戰

1. **多 tag 加權平均**:畫面同時看到 2 個 tag,合併兩者反推位置(降噪)
2. **接 robot_localization EKF(Phase 20A)**:把 AprilTag pose 當 measurement 餵給 EKF,跟 wheel odometry 融合
3. **動態 tag library**:從 yaml 讀 tag 位置,而非 hardcode
4. **發 PoseWithCovariance**:不只發 TF,還發 `geometry_msgs/PoseWithCovarianceStamped`(下游 EKF 才能用 covariance 加權)

---

## 下一步

- [03. YOLO + ROS 2](../03-yolo-ros2/)(待寫) — 從 fiducial 進到一般物件偵測

---

---

## ⏸ 驗證前 audit checklist(留給跑驗證的人)

這章**code 是 inline 範例不是完整 package**,風險最高。跑驗證前要做的事:

- [ ] **先建完整 package**:本章只給 inline localizer 範例,沒給完整 `package.xml` + `CMakeLists.txt`。要驗證得自己包成 package(可以參考 perception/01 結構)
- [ ] **`apriltag_msgs` 在 Humble 是否預裝**:可能要 `sudo apt install ros-humble-apriltag-msgs ros-humble-apriltag-ros`,寫 README 時沒列(疏漏)
- [ ] **AprilTag `family: 36h11` vs SDF 內 tag 圖**:本章假設 Gazebo 有 AprilTag SDF model,但實際 turtlebot3 場景**沒帶 AprilTag**,要自己生 tag mesh 或下載 ar_track_alvar 等替代品
- [ ] **`tf2_ros::TransformBroadcaster` 在 Humble 的 include**:可能是 `<tf2_ros/transform_broadcaster.h>` 或 `<tf2_ros/transform_broadcaster.hpp>`
- [ ] **localizer code 簡化版的旋轉假設**:我寫「假設 tag 與 world 軸平行,只取平移」— 實機完全不會這樣,旋轉計算需要 `tf2::fromMsg` + `transform.inverse()`
- [ ] **TF tree 衝突**(雷 4 寫過):同時發 `world → odom` 跟 `world → base_link` 會打架,跑前確認設計

**這章準備驗證時建議:**先讓 apriltag_node 跑通(看 `/tag_detections` 有資料),再寫 localizer。

---

> **驗證狀態**:⏸ 純文字草稿(2026-05-05) — code 是 inline 簡化範例,**比 perception/01 風險更高**(沒完整 package、旋轉假設不適用實機)。雲端 / WSL 實際驗證後升 ✅。
