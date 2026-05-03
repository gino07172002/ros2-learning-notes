# Phase 20A:Odometry + robot_localization (EKF 融合)

> 把**輪式里程計**(會打滑、有系統性偏差)跟 **IMU**(短期準、長期 drift)用 Extended Kalman Filter 融合,得到比單一 sensor 都準的位姿。SLAM / Nav2 的標準前置。

**學完你會**:
- 寫一個 ROS 2 node 發 `nav_msgs/Odometry`,正確設定 `pose.covariance` / `twist.covariance`
- 寫一個 IMU 發布器,正確設定三個 covariance(orientation / angular_velocity / linear_acceleration)
- 用 `robot_localization` 的 `ekf_node` 融合多個 sensor,寫 YAML config
- 看穿「為什麼 EKF 沒在融合」的常見原因(missing TF、yaml type、sensor 沒 covariance)
- 親眼看 EKF 比 wheel-only 累積誤差小 **8 倍** 的數據

**前置**:
- [Phase 16 TF2](../phase-16-tf2/) — EKF 會發 TF + 需要 sensor frame TF
- [Phase 06 Parameters](../phase-06-parameters/) — EKF 用 YAML 設定上百個參數
- [Phase 10 Launch](../phase-10-launch-files-basics/) — 整個 demo 用 launch 啟動 4 個 node

**產出**:
- [`code/my_cpp_pkg/src/fake_wheel_odom.cpp`](code/my_cpp_pkg/src/fake_wheel_odom.cpp) — 假輪式里程計,線速度 +5%、角速度 -15%
- [`code/my_cpp_pkg/src/fake_imu.cpp`](code/my_cpp_pkg/src/fake_imu.cpp) — 假 IMU,角速度準 + 高頻 noise
- [`code/my_cpp_pkg/src/comparator.cpp`](code/my_cpp_pkg/src/comparator.cpp) — 跟「真值」(知道是定速圓周)比較
- [`code/my_cpp_pkg/config/ekf.yaml`](code/my_cpp_pkg/config/ekf.yaml) — EKF 完整 YAML 設定
- [`code/my_cpp_pkg/launch/ekf_demo.launch.py`](code/my_cpp_pkg/launch/ekf_demo.launch.py)

**環境**:☁️💻 雙環境通用(純文字驗證,不需 GUI)

---

## 為什麼這章重要

**Nav2 的 amcl/dwb_local_planner 都需要一個準確、平滑、不跳變的 `/odom` topic**。

但實機上常見的問題:
- **輪式 odometry 打滑** — 機器人卡到地毯邊、轉彎時內外輪轉速比不對 → yaw 累積誤差
- **IMU 長期 drift** — 雖然短期準,角速度積分一兩分鐘就 yaw 偏 5°
- **單一 sensor 都不夠** — wheel 在 yaw 上爛,IMU 在 position 上爛

EKF 把「各 sensor 在自己擅長的 state 上比較準」這件事數學化:**每個 sensor 提供它的不確定性(covariance),EKF 加權平均後產生比任一 sensor 都更貼近真值的估計**。

業界做法 100% 都是這樣:Nav2 wiki 第一頁就要求 `robot_localization` 在 odom→base_link。

---

## 🏗️ 架構

```
                     ┌──────────────────────────┐
                     │ fake_wheel_odom (20 Hz)  │
                     │ /wheel/odometry          │   ← 線速度 +5%、角速度 -15%
                     │ pose.cov: vx 信任,wz 信任 │
                     └────────────┬─────────────┘
                                  │
                                  ▼
                     ┌──────────────────────────┐
 fake_imu (100 Hz) → │     ekf_node (30 Hz)     │ → /odometry/filtered
 /imu/data           │  Extended Kalman Filter  │   /tf (odom → base_link)
 vyaw 信任,acc 不信 │  融合兩個 sensor 各自最強的 state
                     └──────────────────────────┘
                                  │
                                  ▼
                     ┌──────────────────────────┐
                     │ comparator (1 Hz)        │
                     │ 印出 TRUE / WHEEL / EKF 三者位姿,Δ=與真值距離
                     └──────────────────────────┘
```

`fake_wheel_odom` 跟 `fake_imu` 都知道**「真值」是定速圓周運動 (v=0.5, w=0.3)**,但各自把報告值加上不同的偏差,模擬實機上 sensor 必有的誤差來源。EKF 不知道真值,只能靠兩個 sensor 的 covariance 自己猜。

---

## 💻 重點檔案

### 1. fake_wheel_odom.cpp — covariance 是正確融合的關鍵

完整見 [`code/my_cpp_pkg/src/fake_wheel_odom.cpp`](code/my_cpp_pkg/src/fake_wheel_odom.cpp)。

```cpp
auto &pc = msg.pose.covariance;        // 36 元素(6x6 row-major)
pc.fill(0.0);
pc[0]  = 1e-3;  pc[7]  = 1e-3;  pc[14] = 1e6;     // x: 信任,y: 信任,z: 不信
pc[21] = 1e6;   pc[28] = 1e6;   pc[35] = 1e-2;    // roll/pitch 不信,yaw 略信任

auto &tc = msg.twist.covariance;
tc[0]  = 1e-3;  tc[7]  = 1e6;   tc[14] = 1e6;     // vx 非常信任,vy/vz 不信
tc[21] = 1e6;   tc[28] = 1e6;   tc[35] = 1e-3;    // wz 信任(實際打滑的偏差由 EKF 拒絕)
```

**這個矩陣是 EKF 要不要吃這筆訊息的關鍵**。把不可靠的 state(車不會飛 → z 軸)設成 1e6,EKF 自動忽略;把可靠的設成 1e-3,EKF 給高權重。

### 2. fake_imu.cpp — IMU 的三個 covariance 很容易設錯

```cpp
msg.orientation_covariance =
  {1e6, 0, 0,  0, 1e6, 0,  0, 0, 1e-2};         // 只信 yaw
msg.angular_velocity_covariance =
  {1e6, 0, 0,  0, 1e6, 0,  0, 0, 1e-4};         // 只信 vyaw,且非常信
msg.linear_acceleration_covariance =
  {1e-1, 0, 0,  0, 1e-1, 0,  0, 0, 1e6};        // 加速度有 noise,姑且信
```

**注意**:IMU 的 covariance 第一個元素是 -1 代表「整個欄位無效」,而不是「不信任」。我們明確列出 1e6 才會讓 EKF 知道「有讀數,但不信」。

### 3. ekf.yaml — sensor_config 是 15 元素 boolean array

完整見 [`code/my_cpp_pkg/config/ekf.yaml`](code/my_cpp_pkg/config/ekf.yaml)。

```yaml
ekf_filter_node:
  ros__parameters:
    two_d_mode: true        # 鎖死 z / roll / pitch,只融合 x / y / yaw
    publish_tf: true        # EKF 發 odom→base_link

    # ─── sensor 1: wheel ───
    odom0: /wheel/odometry
    odom0_config: [false,false,false,    # x, y, z         — 不吃 wheel 的 absolute 位置
                   false,false,false,    # roll, pitch, yaw — 不吃 wheel 的 yaw(打滑時很爛)
                   true, false, false,   # vx, vy, vz      — ✅ 只吃線速度
                   false,false,false,    # vroll, vpitch, vyaw
                   false,false,false]    # ax, ay, az

    # ─── sensor 2: IMU ───
    imu0: /imu/data
    imu0_config: [false,false,false,
                  false,false,false,
                  false,false,false,
                  false,false,true,      # vyaw — ✅ IMU 角速度比 wheel 準
                  false,false,false]
```

**設計邏輯**:wheel 給 vx,IMU 給 vyaw。EKF 自己積分得到 x/y/yaw,**完全跳過兩個 sensor 各自不準的 state**。

### 4. ekf_demo.launch.py — 4 個 node 串起來

```python
return LaunchDescription([
    Node(package='tf2_ros', executable='static_transform_publisher',
         arguments=['0','0','0','0','0','0','base_link','imu_link']),
    Node(package='phase20a_pkg', executable='fake_wheel_odom', ...),
    Node(package='phase20a_pkg', executable='fake_imu', ...),
    Node(package='robot_localization', executable='ekf_node',
         parameters=[ekf_yaml]),
    Node(package='phase20a_pkg', executable='comparator', ...),
])
```

`static_transform_publisher` **是必須**——沒這條 EKF 找不到 imu_link → base_link 的轉換,會無聲丟掉所有 IMU 訊息(沒任何錯誤訊息,你只會看到 EKF 不轉彎)。詳見雷 1。

---

## 🚀 完整 Demo 流程(WSL,驗證過)

### Step 1:部署 + build

```bash
rm -rf ~/ros2_ws/src/phase20a_pkg
cp -r /mnt/d/ros_learn/ros2-learning-notes/phase-20A-odometry-ekf/code/my_cpp_pkg \
      ~/ros2_ws/src/phase20a_pkg
sed -i 's|<name>my_cpp_pkg</name>|<name>phase20a_pkg</name>|' \
      ~/ros2_ws/src/phase20a_pkg/package.xml
sed -i 's|project(my_cpp_pkg)|project(phase20a_pkg)|' \
      ~/ros2_ws/src/phase20a_pkg/CMakeLists.txt
source /opt/ros/humble/setup.bash
cd ~/ros2_ws && colcon build --packages-select phase20a_pkg
```

### Step 2:跑 demo 22 秒

```bash
source ~/ros2_ws/install/setup.bash
timeout 22 ros2 launch phase20a_pkg ekf_demo.launch.py 2>&1 | grep -E 'TRUE|WHEEL|EKF '
```

**驗證過的真實輸出**(車繞圓 16 秒):

```
t=  1.0s | TRUE  x= 0.493 y= 0.074 yaw= 0.300
         | WHEEL x= 0.519 y= 0.070 yaw= 0.255  (Δ=0.027)
         | EKF   x= 0.385 y= 0.048 yaw= 0.237  (Δ=0.111)
t=  5.0s | TRUE  x= 1.662 y= 1.549 yaw= 1.500
         | WHEEL x= 1.960 y= 1.471 yaw= 1.275  (Δ=0.308)
         | EKF   x= 1.711 y= 1.513 yaw= 1.437  (Δ=0.061)  ← EKF 已超越 wheel
t= 10.0s | TRUE  x= 0.235 y= 3.317 yaw= 3.000
         | WHEEL x= 1.124 y= 3.775 yaw= 2.550  (Δ=1.000)
         | EKF   x= 0.339 y= 3.465 yaw= 2.937  (Δ=0.181)  ← wheel 已誤差 1m
t= 16.0s | TRUE  x=-1.660 y= 1.521 yaw= 4.800
         | WHEEL x=-1.682 y= 3.265 yaw=-2.203  (Δ=1.744)  ← wheel 完全跑掉
         | EKF   x=-1.768 y= 1.710 yaw=-1.546  (Δ=0.218)  ← EKF 仍貼近真值
```

### Step 3:結論一覽

| 時間 | wheel-only Δ | EKF Δ | EKF 勝過 wheel |
|------|-------------|-------|----------------|
|  1s | 0.03 m | 0.11 m | EKF 還在收斂 |
|  5s | 0.31 m | 0.06 m | **5×** |
| 10s | 1.00 m | 0.18 m | **5.5×** |
| 16s | 1.74 m | 0.22 m | **8×** |

剛開始 EKF 比 wheel 差(它需要幾秒鐘收斂初始狀態),但**累積時間越長,EKF 領先得越多**。
這就是為什麼 SLAM / Nav2 全都用 EKF,沒人單純用 wheel odometry。

---

## ☁️ TheConstructSim

ROSject 已預裝 `robot_localization`,流程一模一樣,只需要先把 phase20a_pkg 部署進 ROSject 的 `~/ros2_ws/src/`(用 `git clone` 或 web UI 上傳)。

唯一差異:雲端 `colcon build` 比 WSL 慢(共享資源)。

---

## 🐛 常見雷

### ⚠️ 雷 1:EKF 沒抱怨,但 yaw 永遠不變(IMU 沒被吃)

**症狀**:`/odometry/filtered` 有發,但 `pose.position.x` 線性增加、`yaw` 永遠 0,EKF 完全不會轉彎。

**原因**:**IMU 的 frame_id 是 `imu_link`,但 TF 樹裡沒有 `imu_link` → `base_link` 的變換**。EKF 預設要把 IMU 從自己 frame 轉到 base_link 才會用,**找不到 TF 就無聲丟掉訊息**(沒 error,沒 warning,你只會看到結果不對)。

**解**:launch 加 `static_transform_publisher`:
```python
Node(package='tf2_ros', executable='static_transform_publisher',
     arguments=['0','0','0','0','0','0','base_link','imu_link'])
```
實機要設真實的 IMU 安裝位置(例如 IMU 在車上 0.1m 高 → `'0','0','0.1','0','0','0','base_link','imu_link'`)。

### ⚠️ 雷 2:`Sequence should be of same type` 解析 yaml 失敗

**症狀**:`ekf_node` 啟動立刻 crash:
```
failed to initialize rcl: Couldn't parse params file:
Error: Sequence should be of same type. Value type 'integer' do not belong at line_num 64
```

**原因**:`process_noise_covariance` 是 15×15 = 225 元素 array,如果寫成 `[0.05, 0, 0, ...]`,YAML parser 會把第一個解成 float、後面的 0 解成 int,**rcl 要求 sequence 內所有 element 同型別**。

**解**:**統一寫成 float**(加 `.0`):
```yaml
process_noise_covariance: [
  0.05, 0.0, 0.0, ...,    # 所有 0 都寫 0.0
]
```

### ⚠️ 雷 3:EKF 起來就吃 90% CPU

**症狀**:啟動 EKF 後 `top` 看 ekf_node 持續 80–90% CPU,跟你預期的「30Hz 不該那麼貴」差很多。

**原因**:你 sensor 訊息發太快(例如 IMU 1000 Hz),EKF queue 處理不完,每個 cycle 都在追訊息。

**解**:
1. IMU 發布頻率降到 100–200 Hz(實機上 IMU 通常 200 Hz 上限)
2. EKF YAML 的 `imu0_queue_size` 設小(我們用 10)
3. 確認 `frequency` 不要設太高(30 Hz 對 ROS 已經很高)

### ⚠️ 雷 4:`covariance` 設錯讓 EKF 不信 / 太信

**症狀**:EKF 輸出的 `/odometry/filtered` 比單一 sensor 還爛。

**原因**:EKF 是「按 covariance 加權平均」的數學工具,**covariance 是它對「這個值該信幾分」的唯一輸入**。
- 設太小(1e-6)→ EKF 完全相信,即使 sensor 在亂報
- 設太大(1e10)→ EKF 完全忽略,等於沒這個 sensor
- 設成 0 → **可能直接觸發數值不穩定**(矩陣不可逆)

**解**:
- 對你不確定的 state 設 1e6(等同忽略)
- 對你信任的 state,從 1e-3 開始試,看結果調
- **所有非對角元素設 0**(假設 state 之間獨立)

### ⚠️ 雷 5:wsl session 結束 background ros2 launch 被殺

**症狀**:用 `nohup` / `setsid` 把 `ros2 launch` 拉背景,wsl 命令一結束 process 全沒。

**原因**:WSL 2 的 systemd-user-session 機制,不像實機 systemd 永生。wsl 命令(對應一個 session)結束後,session 內的 process 即使 detach 也會被一起清掉。

**解**:用 `timeout NN ros2 launch ...` 同步跑完,在這 NN 秒內所有 demo 都完成 + 收 log。本章 README 的 demo 用 `timeout 22` 跑滿 16 秒實驗 + 收尾 6 秒。

更穩的解(實機部署用):**寫成 systemd unit + `Restart=always`**,讓系統管理 launch process。

### ⚠️ 雷 6:EKF 啟動初期(< 2 秒)輸出比單一 sensor 還爛

**症狀**:看 EKF 第一秒的 Δ 比 wheel-only 大,以為 EKF 沒在融合。

**原因**:EKF 需要**幾個 sensor cycle 才能收斂**。剛 init 時內部 covariance 矩陣是預設大值,前幾筆 sensor 訊息只是讓它往正確方向收斂。

**解**:看穩態後的數據,不要拿初始化期的數據比較。教學上明確指出「t<2s 是 init 期」。
真實的成功標準:**t > 5s 後 EKF 持續且大幅勝過單 sensor**——本章 5s 後 EKF 已領先 5 倍,這是預期行為。

---

## 🎯 學到的關鍵概念

| 概念 | 一句話 |
|------|------|
| Odometry 訊息結構 | `nav_msgs/Odometry` 帶 pose + twist + 兩個 36 元素 covariance(6×6) |
| IMU 訊息三個 covariance | orientation / angular_velocity / linear_acceleration,每個 9 元素(3×3) |
| `two_d_mode: true` | 輪式車鎖死 z/roll/pitch,只融合 x/y/yaw,大幅減 EKF 計算 |
| `xxx_config` 15 元素 | EKF 對每個 sensor 用 boolean 矩陣選「吃這個 state 嗎」 |
| `publish_tf` | EKF 自己發 odom→base_link,**記得關掉 sensor 自己發 TF** |
| 缺 IMU TF 是無聲失敗 | 沒 imu_link → base_link 變換,EKF 不會抱怨,只是不用 IMU |
| EKF 勝在累積誤差 | 每個 sensor 短期都還 OK,長期 EKF 拉開差距 8x 以上 |

---

## 🌟 進階挑戰

1. **加 GPS** — 用 `navsat_transform_node` 把 GPS lat/lon 轉成 odom frame,EKF 第三個 sensor。實機戶外用 EKF + GPS 是 Nav2 標準做法
2. **改成 UKF** — `robot_localization` 也提供 `ukf_node`(Unscented Kalman Filter),非線性運動更準。比 EKF 慢但對劇烈轉彎更穩
3. **outlier rejection** — 設 `Mahalanobis distance threshold`,自動丟掉跟預測差太多的 sensor 訊息(實機上 IMU 偶爾抽筋會發 100g 訊號)
4. **錄 bag + 後處理** — 錄一段真實機器人的 wheel + IMU bag,離線跑 EKF,fine-tune covariance(這是 robotics 工程師日常工作)

---

## 🔗 下一步

- **Phase 21A SLAM** — slam_toolbox 吃 `/odometry/filtered`(本章輸出)+ `/scan` 即時建圖
- **[Phase 16 TF2](../phase-16-tf2/)** — 回頭看 EKF 發的 odom→base_link TF 怎麼跟 map→odom 接上
- **Phase 22A Nav2** — Nav2 的整套 stack 都假設你有一個準確的 EKF odom

---

## 📁 完整檔案結構

```
phase-20A-odometry-ekf/
├── README.md                                ← 本檔案
├── code/
│   └── my_cpp_pkg/
│       ├── package.xml
│       ├── CMakeLists.txt
│       ├── src/
│       │   ├── fake_wheel_odom.cpp           ← 假輪式 odometry,vx +5%、wz -15%
│       │   ├── fake_imu.cpp                  ← 假 IMU,vyaw 準 + noise
│       │   └── comparator.cpp                ← 跟「真值」比較,印出 Δ
│       ├── config/
│       │   └── ekf.yaml                      ← EKF 完整設定(225 元素 process noise)
│       └── launch/
│           └── ekf_demo.launch.py            ← 5 個 node:static_tf + 兩 sensor + ekf + comparator
└── images/                                  ← (之後補:rqt_plot odom path 截圖)
```
