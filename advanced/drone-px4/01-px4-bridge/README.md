# 01. PX4 Bridge — SITL + micro-XRCE-DDS

> 從這裡開始無人機之旅！不用購買昂貴的實體飛控板，我們將在模擬環境中啟動 PX4 飛控軟體 (SITL)，並透過 `micro-XRCE-DDS Agent` 建立通訊橋樑，讓 ROS 2 能直接「聽到」飛控板的心跳。

**學完你會**:
- 啟動 PX4 SITL (Software-In-The-Loop) 加上 Gazebo Classic 中的 Iris 無人機模型
- 啟動 `micro-XRCE-DDS Agent`，打通 PX4 與 ROS 2 之間的任督二脈
- 使用標準的 `ros2 topic list` 與 `ros2 topic echo` 讀取無人機的姿態、GPS、電池等遙測資料 (Telemetry)
- 了解為何業界拋棄了 ROS 1 時代的 MAVROS，全面轉向 DDS Bridge

**前置**:
- [Phase 03 Subscriber](../../../phase-03-subscriber-lidar-brake/) — ROS 2 Topic 基礎
- [Phase 17 Gazebo](../../../phase-17-gazebo/) — 模擬器基礎

**環境**:💻 本機 WSL2 / Ubuntu (TheConstructSim 雲端不支援編譯 PX4 核心韌體)

---

## 📍 為什麼要用 micro-XRCE-DDS Bridge？

在 ROS 1 的時代，無人機工程師都是靠 `MAVROS` 這個肥大的節點來與 PX4 溝通。MAVROS 的工作是把 PX4 專屬的 MAVLink 封包，翻譯成 ROS 1 的 Topic。這不僅耗費 CPU 效能，還經常發生漏封包、延遲過高的問題。

到了 ROS 2，因為底層本身就是 DDS (Data Distribution Service)，PX4 官方做了一個革命性的決定：**讓飛控板直接講 DDS**。

透過一個輕量級的 `micro-XRCE-DDS Agent`，PX4 內部的 uORB 訊息會直接被對應成 ROS 2 的 Topic。不再需要第三方翻譯官，傳輸極速、延遲極低。這是當今（PX4 v1.14 以後）的業界絕對標準。

---

## 💻 步驟 1: 環境準備 (極度重要)

PX4 的編譯環境非常龐大，請確保你的 WSL2 或 Ubuntu 有至少 10GB 的剩餘空間。

### 1.1 安裝 PX4 SITL 與 Gazebo
打開終端機（這一步會跑很久，因為會下載並編譯整個飛控韌體）：
```bash
cd ~
git clone https://github.com/PX4/PX4-Autopilot.git --recursive
cd PX4-Autopilot
# 安裝所有系統相依套件
bash ./Tools/setup/ubuntu.sh
# 重新登入或 source ~/.profile 以確保環境變數生效

# 編譯 SITL 並且啟動 Gazebo Classic
make px4_sitl gazebo-classic
```
如果你看到一個藍色的 Gazebo 畫面，且地上有一台 Iris 四軸無人機，恭喜你，你的「大腦」已經上線了！
(你可以按 `Ctrl+C` 先把它關掉)

### 1.2 安裝 micro-XRCE-DDS Agent
這是 ROS 2 與 PX4 溝通的橋樑：
```bash
sudo apt update
sudo apt install ros-humble-micro-xrce-dds-agent
```

### 1.3 編譯 `px4_msgs`
ROS 2 需要知道 PX4 傳過來的訊息長什麼樣子（例如 `VehicleOdometry`, `SensorGps`），所以我們必須在 ROS 2 Workspace 裡編譯這些自訂介面。
```bash
cd ~/ros2_ws/src
git clone https://github.com/PX4/px4_msgs.git -b release/1.14
cd ~/ros2_ws
colcon build --packages-select px4_msgs
source install/setup.bash
```
> ⚠️ **新手雷區 #1**：`px4_msgs` 的分支 (branch) 必須與你下載的 `PX4-Autopilot` 版本完全一致。如果你用 PX4 v1.14，就必須 clone `release/1.14` 的 `px4_msgs`。只要差一個版本，DDS Bridge 就會因為訊息結構不同而完全無法通訊！

---

## 🚀 步驟 2: 跑 Demo (打通天地線)

我們需要開三個終端機，依序啟動：

### 終端機 1：啟動 Micro-XRCE-DDS Agent
這個 Agent 會在 UDP port 8888 監聽 PX4 連線：
```bash
source /opt/ros/humble/setup.bash
MicroXRCEAgent udp4 -p 8888
```

### 終端機 2：啟動 PX4 SITL 模擬器
```bash
cd ~/PX4-Autopilot
make px4_sitl gazebo-classic
```
當 Gazebo 畫面出現後，注意看終端機的文字。你應該會看到 PX4 的大腦（pxh shell）啟動了，並且會印出 `[uxrce_dds_client] connected to server`，這代表 PX4 成功連上剛剛開的 Agent！

### 終端機 3：從 ROS 2 偷聽遙測資料
打開一個全新的終端機，確保 source 了我們的 workspace：
```bash
source /opt/ros/humble/setup.bash
source ~/ros2_ws/install/setup.bash

# 查看現在多了哪些 Topic
ros2 topic list
```

預期你應該會看到一長串 `/fmu/out/` 開頭的 Topic：
```text
/fmu/out/sensor_gps
/fmu/out/vehicle_local_position
/fmu/out/vehicle_odometry
/fmu/out/vehicle_status
...
```

讓我們偷聽一下無人機目前的 GPS 狀態：
```bash
ros2 topic echo /fmu/out/sensor_gps
```
你會看到源源不絕的經緯度與高度資料噴出來！這證明了從「Gazebo 物理引擎 -> PX4 飛控演算法 -> micro-XRCE-DDS -> ROS 2 Topic」的整條鏈路已經完全打通！

---

## 🐛 常見雷

### ⚠️ 雷 1：`MicroXRCEAgent` 指令找不到
**症狀**：打 `MicroXRCEAgent udp4 -p 8888` 出現 `command not found`。
**解**：確保你安裝了 `ros-humble-micro-xrce-dds-agent`，並且執行前有 `source /opt/ros/humble/setup.bash`，這個執行檔是包在 ROS 2 環境裡的。

### ⚠️ 雷 2：PX4 啟動了，但 ROS 2 裡面 `topic list` 空空如也
**症狀**：終端機 2 卡在 pxh shell，但終端機 1 的 Agent 完全沒有顯示 connected。
**原因**：這通常發生在 WSL2 環境，PX4 內部的網路設定抓錯 IP，或是 UDP port 8888 被防火牆擋住。
**解**：在 PX4 pxh shell 中手動輸入 `uxrce_dds_client start -t udp -p 8888` 重新啟動 client 看看報什麼錯。

---

## 🎯 學到的關鍵概念

- **SITL (Software-In-The-Loop)**：無人機開發的核心法則。我們執行的 PX4 韌體與燒錄進真實飛控板上的程式碼是**100% 完全一樣的**。在 SITL 測過的邏輯，到實機上通常八九不離十。
- **`/fmu/out/` 與 `/fmu/in/`**：PX4 官方的 Topic 命名慣例。`/fmu/out/` 代表飛控丟出來的資料（遙測、狀態）；`/fmu/in/` 則是留給我們從 ROS 2 丟控制指令進去的入口。

---

## 🌟 進階挑戰

1. **訂閱 `vehicle_local_position`**：寫一個簡單的 ROS 2 Python Node，訂閱 `/fmu/out/vehicle_local_position`，並在終端機印出無人機目前在 X、Y、Z 軸的座標位置。
2. **用 Foxglove 視覺化**：啟動 `foxglove_bridge`，在 Foxglove 儀表板上畫出無人機目前的高程曲線。

---

## 🔗 下一步
環境打通後，我們終於可以寫程式控它了！進入 [02. Offboard Control](../02-offboard-control/)，我們將寫一個 Node 傳送起飛指令，讓無人機離開地面。
