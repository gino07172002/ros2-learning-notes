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

## 📋 各章雲端可用性對照表

> 想用 TheConstruct 免費 ROSject 跑完課程不用花力氣裝環境的話,先看這張表知道**哪些章節雲端能跑、哪些必須本機**。

**符號**:
- ☁️ **雲端可跑** — TheConstruct 免費 ROSject 內可完整完成,有具體指令
- 🟡 **雲端可跑但效能限制** — 結構上能跑,但雲端 GPU/網路會影響真實體驗(常見於 SLAM/Nav2/MoveIt)
- 🚫 **本機限定** — 雲端跑不了或意義不大(Docker、CI、多機通訊、實機)
- 📚 **觀念章** — 純文字,不需執行環境

### Part 1:通訊基礎

| Phase | 主題 | 雲端? | 雲端用什麼 ROSject / 場景 |
|-------|------|------|-------------------------|
| 01 | 第一支 Publisher | ☁️ | 任意 ROS 2 Humble ROSject + OriginBot 場景 (`/originbot_1/cmd_vel`) |
| 02 | ROS 2 設計哲學 | 📚 | 不需執行 |
| 03 | Subscriber + 光達 | ☁️ | OriginBot 場景(自帶 Livox 3D 光達 `/livox/lidar`) |
| 04 | Service 開關 | ☁️ | 同 Phase 03 OriginBot 場景 |

### Part 2:工具與治理

| Phase | 主題 | 雲端? | 雲端用什麼 ROSject / 場景 |
|-------|------|------|-------------------------|
| 05 | Debug 工具 | ☁️ | 任意 ROSject(rqt_graph、ros2 bag 在雲端網頁版都能用)|
| 06 | Parameters | ☁️ | OriginBot 場景(YAML 載入 + rqt_reconfigure)|
| 07 | Mini Capstone 1 | ☁️ | 任意 ROS 2 ROSject(可用 turtlesim 取代 OriginBot)|

### Part 3:系統設計

| Phase | 主題 | 雲端? | 雲端用什麼 ROSject / 場景 |
|-------|------|------|-------------------------|
| 08 | Custom Interfaces | ☁️ | 任意 ROS 2 ROSject |
| 09 | Executors / Lifecycle / Composition | ☁️ | 任意 ROS 2 ROSject(純 code 演示)|
| 10 | Launch Files 基礎 | ☁️ | 任意 ROS 2 ROSject |
| 11 | Launch Files 進階 | ☁️ | 任意 ROS 2 ROSject |
| 12 | Testing | ☁️ | 任意 ROS 2 ROSject(`colcon test`)|
| 13 | Actions 進階 | ☁️ | 任意 ROS 2 ROSject |
| 14 | 🎯 Capstone 1 | ☁️ | 任意 ROS 2 ROSject |

### Part 4:機器人形體

| Phase | 主題 | 雲端? | 雲端用什麼 ROSject / 場景 |
|-------|------|------|-------------------------|
| 15 | URDF | ☁️ | 任意 ROS 2 ROSject(用 RViz 看 TF tree)|
| 16 | TF2 | ☁️ | 任意 ROS 2 ROSject |
| 17 | Gazebo | 🟡 | TheConstruct 有 TurtleBot3 ROSject(Gazebo Classic 預載),雲端 Gazebo 比 WSL 順 |
| 18 | ros2_control | ☁️ | 任意 ROS 2 ROSject(本章用 mock_components,**不需要實機**)|
| 19 | pluginlib | ☁️ | 任意 ROS 2 ROSject(純 C++ plugin 機制)|
| 20 | 多機通訊 | 🚫 | 雲端 ROSject 內部沒原生 Docker daemon,跑不了 docker compose 模擬多機 |

### Part 5:領域應用

| Phase | 主題 | 雲端? | 雲端用什麼 ROSject / 場景 |
|-------|------|------|-------------------------|
| 20A | Odometry + EKF | ☁️ | 任意 ROS 2 ROSject(純文字驗證,不需 GUI)|
| 21A | SLAM | 🟡 | TheConstruct 的 TurtleBot3 World ROSject(雲端 GPU 可建出地圖,WSL 通常不行)|
| 22A | Nav2 入門 | 🟡 | 同 Phase 21A 的 TurtleBot3 World ROSject |
| 23A | Nav2 BT plugin | ☁️ | 任意 ROS 2 ROSject(純 C++ plugin 編譯 + gtest)|
| 30 | Nav2 BT 進階 | ☁️ | 任意 ROS 2 ROSject(純 BT plugin + gtest)|
| Capstone A | Mobile 整合 | 🟡 | TurtleBot3 World ROSject(雲端 Gazebo + Nav2 + 自訂 BT)|
| 20B | 手臂 URDF | ☁️ | 任意 ROS 2 ROSject(RViz 看模型)|
| 21B | MoveIt Setup Assistant | 🚫 | 純 GUI wizard,雲端 web terminal 跑不順,**強烈建議本機** |
| 22B | MoveIt C++ | ☁️ | 任意 ROS 2 ROSject(純文字驗證,4 種 plan)|

### Part 6:生產化部署

| Phase | 主題 | 雲端? | 雲端用什麼 ROSject / 場景 |
|-------|------|------|-------------------------|
| 24 | Docker 化 | 🚫 | 雲端 ROSject 內部沒原生 Docker daemon,**必須本機** |
| 25 | CI/CD | 🚫 | GitHub Actions 跑在 GitHub 雲,跟 TheConstruct 無關 |
| 26 | DDS QoS | ☁️ | 任意 ROS 2 ROSject(純 ROS 2 內 QoS 行為)|
| 32 | rosbag2 進階 | ☁️ | 任意 ROS 2 ROSject |
| 35 | Foxglove Bridge | ☁️ | 任意 ROS 2 ROSject(雲端 expose port 給 Foxglove web app)|
| 36 | Diagnostics + Watchdog | ☁️ | 任意 ROS 2 ROSject |
| 37 | Lifecycle + Diagnostics | ☁️ | 任意 ROS 2 ROSject |
| Capstone Final | Docker 化 mobile robot | 🚫 | 雲端沒 Docker,只能本機跑;但**內含的 Capstone A** 可用 TurtleBot3 ROSject 跑 |

---

## 💰 TheConstruct 免費資源怎麼用最划算

> ⚠️ **注意**:TheConstruct 的免費政策可能變動,以下資訊以 2026 年初為準,實際以官網公告為主。

### 免費版能用什麼

| 資源 | 免費版限制 |
|------|-----------|
| **ROSjects(雲端工作區)** | 可建多個,但每次連線時間有限(常見 1 小時/次) |
| **預裝環境** | ROS 2 Humble + Gazebo Classic 11 + RViz + 大部分常用套件 |
| **預載機器人場景** | OriginBot(賽車型,3D 光達)/ TurtleBot3 / 多種其他 |
| **網頁 Terminal / VS Code Editor / Gazebo viewer** | 都免費 |

### 不適合雲端做的事(就算免費也別硬上)

- ❌ **跨多個 container 跑 Docker** — 雲端 ROSject 是單一 sandbox
- ❌ **長時間實驗(過夜跑)** — 連線會自動 timeout
- ❌ **裝額外 apt 套件** — 免費版不能 sudo apt install(付費版才行)
- ❌ **改 systemd / kernel 級設定** — 沙盒隔離
- ❌ **多機 ROS_DOMAIN_ID 隔離測試** — 一個 ROSject 是一個沙盒

### 建議使用節奏

```
Part 1–3(Phase 01–14)   → ☁️ 全雲端跑,免裝環境
Part 4 觀念章(15、16、19) → ☁️ 雲端,簡單
Part 4 形體章(17、18)    → ☁️ 雲端 Gazebo 比 WSL 順
Part 5 SLAM/Nav2(21A、22A)→ ☁️ 雲端必選(WSL 沒 GPU)
Part 5 MoveIt Track B    → 本機(21B)+ 雲端(22B)混用
Part 6 Docker/CI/實機    → 💻 必須本機
```

**結論**:**前期 TheConstruct,中後期切本機,SLAM/Nav2 階段回雲端跑真 demo**。

---

## 🤔 該選哪一個開始?

```
你的情境                            建議起點
─────────────────────────────────────────────
完全沒摸過 ROS                       → TheConstruct
不想花時間裝 ROS 2 / Gazebo          → TheConstruct
裝過 Linux、會 apt                   → 本機 WSL2 直接開幹
有 Mac/Linux 主機                    → 本機(Mac 用 Docker,Linux 直接裝)
公司電腦不能裝東西                    → TheConstruct
要做專題、發 GitHub repo              → 本機(git 操作方便)
時間少、只想體驗                      → TheConstruct
要做畢業專題、未來想找機器人工作       → 本機(業界用本機)
要學 SLAM / Nav2 但電腦沒獨顯         → TheConstruct(雲端有 GPU)
要學 Docker / 部署 / CI               → 本機(雲端做不了)
```
