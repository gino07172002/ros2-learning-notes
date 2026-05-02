# 環境設定：TheConstructSim vs 本機 WSL2

本路徑的程式碼可以在**兩種環境**執行。各有優缺點，建議先看完比較表再選。

---

## 🆚 兩種環境快速比較

| 面向 | TheConstructSim (雲端) | 本機 WSL2 (Ubuntu 22.04) |
|------|----------------------|--------------------------|
| **安裝時間** | 0 分鐘（註冊即用） | 30–60 分鐘 |
| **費用** | 免費版有限制，付費較貴 | 完全免費 |
| **離線可用** | ❌ 需網路 | ✅ |
| **效能** | 雲端伺服器，網路延遲 | 看你的電腦，無延遲 |
| **GPU 加速** | 預設無 | 看顯卡（WSLg 支援） |
| **Gazebo 模擬** | 預先載入機器人場景 | 自己 spawn |
| **存檔** | 雲端 ROSject | 本機檔案 |
| **學習曲線** | 即時上手 | 要會 WSL + apt 基礎 |
| **多機實驗** | 受限 | 自由（多開 WSL distro） |
| **適合誰** | 想快速體驗、不想配環境 | 想長期開發、上機部署 |

**建議**：**兩個都會用**。前期（Phase 01–04）TheConstruct 學 ROS 2 觀念效率高；後期（Phase 12+）本機跑 Gazebo + Nav2 / MoveIt 才有彈性。

---

## ☁️ TheConstructSim 操作流程

### 一次性設定

1. 註冊 [The Construct](https://app.theconstructsim.com/)
2. 進入 **ROSjects** → **Create New ROSject**
3. ROS Distro 選 **ROS 2 Humble**
4. 點擊 `</> Open` 進入虛擬機

### 每次開發流程

每次回到 ROSject，環境已經保留你上次的檔案。

```bash
# 1. 開 Terminal（介面下方工具列）
# 2. 預設工作區是 ~/ros2_ws，進去看狀態
cd ~/ros2_ws

# 3. 用 Code Editor 開檔案編輯（介面下方）
#    路徑：~/ros2_ws/src/<你的套件>/

# 4. 編譯
colcon build --packages-select my_cpp_pkg
source install/setup.bash

# 5. 執行
ros2 run my_cpp_pkg auto_drive

# 6. 看 Gazebo 視窗（介面下方）就會看到車子動
```

### TheConstruct 特有概念

| 概念 | 說明 |
|------|------|
| **ROSject** | 一個雲端虛擬機 + 預設場景 + 你的 code，可重啟保留狀態 |
| **預載機器人** | 場景內常已 spawn 好機器人（如 OriginBot），不需自己起 |
| **Topic 命名前綴** | 多車場景下 topic 會帶前綴，例 `/originbot_1/cmd_vel` |
| **Web Terminal** | 介面下方的 terminal、code editor、Gazebo 都是 web 內嵌 |

### 限制

- 免費帳號每次連線時間有限（通常 1 小時）
- 雲端 GPU 有限，重型模擬會卡
- Gazebo 場景固定，難自訂 world
- 套件預先裝好，無法 `apt install` 新東西（除非付費 plan）

---

## 💻 本機 WSL2 操作流程

### 一次性設定（首次用）

完整安裝流程見 **[`SETUP-WSL.md`](SETUP-WSL.md)**（建立中）。摘要：

```bash
# 1. 設 locale
sudo apt update && sudo apt install -y locales
sudo locale-gen en_US en_US.UTF-8
sudo update-locale LC_ALL=en_US.UTF-8 LANG=en_US.UTF-8

# 2. 加 ROS 2 apt source
sudo apt install -y software-properties-common curl
sudo add-apt-repository universe
sudo curl -sSL https://raw.githubusercontent.com/ros/rosdistro/master/ros.key \
  -o /usr/share/keyrings/ros-archive-keyring.gpg
echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/ros-archive-keyring.gpg] http://packages.ros.org/ros2/ubuntu $(. /etc/os-release && echo $UBUNTU_CODENAME) main" \
  | sudo tee /etc/apt/sources.list.d/ros2.list > /dev/null
sudo apt update

# 3. 安裝（這步會下載約 1.5 GB）
sudo apt install -y ros-humble-desktop ros-dev-tools \
  python3-rosdep python3-colcon-common-extensions

# 4. 永久 source 環境
echo "source /opt/ros/humble/setup.bash" >> ~/.bashrc

# 5. （Mobile Track 才需要）裝 turtlebot3 + Gazebo
sudo apt install -y ros-humble-turtlebot3* gazebo

# 6. 建立工作區
mkdir -p ~/ros2_ws/src
cd ~/ros2_ws && colcon build
echo "source ~/ros2_ws/install/setup.bash" >> ~/.bashrc
```

### 每次開發流程

```bash
# 1. 開啟 WSL Ubuntu（Windows 開始選單 → Ubuntu）
# .bashrc 已自動 source ROS 2 環境

# 2. 把該章的 code 複製進工作區
cp -r /mnt/d/ros_learn/ros2-learning-notes/phase-XX-xxx/code/my_cpp_pkg ~/ros2_ws/src/

# 3. 編譯
cd ~/ros2_ws
colcon build --packages-select my_cpp_pkg
source install/setup.bash

# 4. 啟動模擬器（範例：turtlesim）
ros2 run turtlesim turtlesim_node    # 跳出視窗 (透過 WSLg)

# 5. 另開一個 Ubuntu terminal 跑你的程式
ros2 run my_cpp_pkg auto_drive --ros-args -r cmd_vel:=/turtle1/cmd_vel
```

### 本機 GUI（WSLg）

Windows 11 + WSL2 預設有 **WSLg**——Linux GUI 程式直接彈到 Windows 桌面，不需要額外裝 X server。

驗證 WSLg 可用：
```bash
sudo apt install -y mesa-utils x11-apps
xeyes              # 應該跳出一對眼睛跟著滑鼠
glxinfo | grep "OpenGL renderer"  # 應該顯示顯卡資訊
```

如果 `xeyes` 跳不出來：你的 Windows 版本可能太舊（需 Win11 或 Win10 21H2+）。

### 本機特有概念

| 概念 | 說明 |
|------|------|
| **WSL distro** | Ubuntu 是一個 distro，可以裝多個（測多機通訊用）|
| **`/mnt/d/`** | WSL 內存取 Windows D: 的路徑，方便共享檔案 |
| **WSLg** | 自動把 Linux GUI 投到 Windows 桌面 |
| **`source setup.bash`** | 每個新 terminal 都要 source（已寫進 `~/.bashrc`）|
| **TURTLEBOT3_MODEL** | 用 turtlebot3 必設環境變數：`export TURTLEBOT3_MODEL=burger` |

### 優勢

- 全離線、無時間限制
- 可以自己 spawn 任何 URDF / world
- 可以裝任何 apt 套件、Python lib
- 可以直接 docker、CI、push 上 GitHub
- 性能比雲端流暢（看你電腦）

### 常見坑

| 症狀 | 原因 | 解法 |
|------|------|------|
| `colcon build` 報 `command not found` | 沒 source 環境 | `source /opt/ros/humble/setup.bash` |
| Gazebo 開啟黑畫面 | OpenGL 驅動問題 | 更新 Windows 顯卡驅動（NVIDIA / Intel）|
| RViz2 卡頓 | WSLg 渲染瓶頸 | 降 RViz 顯示元件數，或考慮直接裝 native Linux |
| 跨機器人通訊收不到 | DDS Discovery 範圍 | 確認同 `ROS_DOMAIN_ID`，必要時用 Discovery Server |
| 編譯後 `ros2 run` 找不到套件 | 沒 source `install/setup.bash` | `source ~/ros2_ws/install/setup.bash` |

---

## 🔀 兩種環境的程式碼通用性

**本路徑所有 phase 的 code 兩種環境都能直接跑**，差異只在「執行時的 remapping」：

```bash
# TheConstructSim（OriginBot 在原始 topic 上）
ros2 run my_cpp_pkg auto_drive --ros-args -r cmd_vel:=/originbot_1/cmd_vel

# 本機 WSL2 + turtlesim
ros2 run my_cpp_pkg auto_drive --ros-args -r cmd_vel:=/turtle1/cmd_vel

# 本機 WSL2 + turtlebot3 Gazebo
ros2 run my_cpp_pkg auto_drive    # turtlebot3 預設訂閱 /cmd_vel，不用 remap
```

這就是為什麼 [Phase 01](phase-01-cloud-env-first-publisher/) 開始就教 **「程式碼用相對 topic 名稱、執行時 remap」**——同一份 code 兩種環境通吃。

---

## 📋 各章建議的環境

| Phase | 推薦環境 | 為什麼 |
|-------|---------|--------|
| 01–04（通訊基礎） | TheConstruct ✅ | 預載機器人省事，學觀念為主 |
| 05（Debug 工具） | 兩者皆可 | rqt 在哪都能跑 |
| 06–08（Param/Custom Msg/Executor） | 兩者皆可 | 純 ROS 觀念 |
| 09（Launch） | 本機 ✅ | 自己組多節點 launch 比較有感 |
| 10（測試） | 本機 ✅ | CI 整合需要本機 |
| 11–13（Action/URDF/TF2） | 本機 ✅ | URDF 自訂機器人 TheConstruct 受限 |
| 14（Gazebo 整合） | 本機 ✅ | TheConstruct 隱藏掉這層 |
| 15（ros2_control） | 本機 ✅ | 連硬體必須本機 |
| 16+（Track A/B 應用） | 本機 ✅ | Nav2/MoveIt 大型實驗，雲端會卡 |
| 20–24（生產化） | 本機 ✅ | Docker、CI、實機部署都需要本機 |

**結論**：**前期 TheConstruct，中後期切到本機**。建議第 9 章前後切換，這時你 ROS 2 觀念已穩，本機環境學起來不會吃力。

---

## 🤔 該選哪一個開始？

```
你的情境                            建議起點
─────────────────────────────────────────────
完全沒摸過 ROS                       → TheConstruct
裝過 Linux、會 apt                   → 本機 WSL2 直接開幹
有 Mac/Linux 主機                    → 本機（Mac 用 Docker，Linux 直接裝）
公司電腦不能裝東西                    → TheConstruct
要做專題、發 GitHub repo              → 本機（git 操作方便）
時間少、只想體驗                      → TheConstruct
要做畢業專題、未來想找機器人工作       → 本機（業界用本機）
```
