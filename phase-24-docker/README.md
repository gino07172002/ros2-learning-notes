# Phase 24:Docker 化 Capstone 1

> 把 **Phase 14 Capstone(ApproachController)** 包成 Docker image,用 `docker compose up` 一鍵起兩個 container 互通。

**學完你會**:
- 寫 multi-stage Dockerfile,**builder stage colcon build / runtime stage 只帶 install/**(image 體積壓到 < 800MB)
- 設計 entrypoint.sh 讓 container 自動 source ROS 環境並正確接 SIGTERM
- 用 `docker compose` 起多 container 跑 ROS 系統,**搞懂為什麼 bridge network 會看得到 topic 卻收不到資料**
- 跨 container 跑 Topic / Service / Action,真實看到 DDS 在 container 之間運作

**前置**:
- [Phase 14 Capstone 1](../phase-14-capstone-1/) — 我們要 Docker 化的對象
- [Phase 26 DDS QoS](../phase-26-dds-qos/) — 看完才知道 sensor data 為什麼是 BestEffort
- 對 Docker / docker compose 基本操作熟悉(`docker build`、`docker run`、volume、network)

**產出**:[`code/docker-capstone/`](code/docker-capstone/) — Dockerfile、docker-compose.yml、entrypoint.sh、fake_lidar.py

**環境**:💻 本機 WSL2(Docker Desktop 或 docker CE 都可)
> ☁️ TheConstructSim:**沒有 Docker**(它本身就是 container 環境)。改用「直接在 ROSject 裡跑 Capstone」即可,Phase 14 已經有那條路徑。下方有「兩個環境的對應關係」說明。

---

## 🤔 為什麼這章重要

ROS 2 專案要交付,**Docker 是業界標配**。理由:

1. **可重現** — `docker run` 在哪台機器都長一樣,不用「在我電腦會跑」
2. **隔離 ROS distro** — 同一台機器上 Humble、Iron、Jazzy 共存
3. **CI 友善** — Phase 25 的 GitHub Actions 就是在 container 裡跑
4. **生產部署** — Pi、Jetson、車機都可以 docker pull image,不用每台都 colcon build

但 Docker + ROS 2 有**幾個非典型 Docker 知識的雷**(DDS / multicast / SHM),這章會把它們全部踩過一輪。

---

## 🏗️ 整體架構

```
┌─ docker-compose.yml ─────────────────────────────────────────┐
│                                                              │
│   service: controller         service: lidar                 │
│   ┌────────────────────┐      ┌─────────────────────┐        │
│   │ image: capstone1   │      │ image: capstone1    │        │
│   │ ros2 launch        │◀━━━━▶│ python3             │        │
│   │   capstone.launch  │  DDS │   fake_lidar.py 0.4 │        │
│   │ • Lifecycle node   │      │ • PointCloud2 pub   │        │
│   │ • Action server    │      │   @ 10Hz BestEffort │        │
│   │ • Service server   │      │                     │        │
│   └────────────────────┘      └─────────────────────┘        │
│           ▲                                                  │
│           │  network_mode: host  (DDS multicast 用)          │
│           │  ipc: shareable      (SHM transport 跨 container) │
│           │  ROS_DOMAIN_ID=42    (跟 host 預設 0 隔離)         │
│           ▼                                                  │
└──────────────────────────────────────────────────────────────┘
```

兩個 container **共用同一個 image**,但啟動指令不同 — 這比每個 service 一個 image 省 build 時間 + 省磁碟。

---

## 💻 重點檔案

### 1. Dockerfile — Multi-stage build

完整檔案見 [`code/docker-capstone/Dockerfile`](code/docker-capstone/Dockerfile)。

```dockerfile
# Stage 1: builder — 完整 ros:humble,colcon build 套件
FROM ros:humble AS builder
WORKDIR /ws
COPY phase-08-custom-interfaces/code/my_robot_interfaces  src/my_robot_interfaces
COPY phase-14-capstone-1/code/my_cpp_pkg                  src/phase14_capstone
RUN sed -i 's|<name>my_cpp_pkg</name>|<name>phase14_capstone</name>|' \
        src/phase14_capstone/package.xml \
 && sed -i 's|project(my_cpp_pkg)|project(phase14_capstone)|' \
        src/phase14_capstone/CMakeLists.txt \
 && sed -i "s|package='phase14_pkg'|package='phase14_capstone'|" \
        src/phase14_capstone/launch/capstone.launch.py
RUN /bin/bash -c "source /opt/ros/humble/setup.bash && \
                  colcon build --packages-select my_robot_interfaces phase14_capstone \
                               --cmake-args -DBUILD_TESTING=OFF"

# Stage 2: runtime — 較小的 ros-core,只帶 builder 的 install/
FROM ros:humble-ros-core
COPY --from=builder /ws/install /ws/install
COPY phase-24-docker/code/docker-capstone/entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh
ENTRYPOINT ["/entrypoint.sh"]
CMD ["ros2", "launch", "phase14_capstone", "capstone.launch.py"]
```

**為什麼分兩 stage**:`ros:humble`(完整版)約 2GB、含 dev tools,builder 用它編譯。runtime 切到 `ros:humble-ros-core`(約 600MB),只 `COPY --from=builder` 需要的 `install/`,**最終 image 比單階段小 60–70%**。

**為什麼 build context 是 repo root**:Dockerfile 要讀 sibling phase(`phase-08-` 跟 `phase-14-`)的 code。`docker compose` 的 `build.context` 設成 `../../..` 就是這意思。配合 `.dockerignore` 白名單,只把這兩個 phase + Phase 24 自己的檔案傳給 daemon。

### 2. entrypoint.sh — 自動 source 環境 + 正確訊號傳遞

完整檔案見 [`code/docker-capstone/entrypoint.sh`](code/docker-capstone/entrypoint.sh)。

```bash
#!/bin/bash
set -e
source /opt/ros/humble/setup.bash
source /ws/install/setup.bash
echo "[entrypoint] ROS_DOMAIN_ID=${ROS_DOMAIN_ID:-0}"
echo "[entrypoint] exec: $*"
exec "$@"             # ← 關鍵:exec 把 PID 1 交給真正的 process
```

**為什麼用 `exec`**:沒 `exec` 的話 entrypoint 自己是 PID 1,真正 process 是 PID 2,`docker stop` 送的 SIGTERM 只到 PID 1,**ROS node 收不到關閉訊號**,只能等 10 秒 timeout 後 SIGKILL。用 `exec "$@"` 把當前 shell 替換成目標 process,SIGTERM 直達。

### 3. docker-compose.yml — 兩個 service + DDS 設定

完整檔案見 [`code/docker-capstone/docker-compose.yml`](code/docker-capstone/docker-compose.yml)。

關鍵三行(在每個 service 都要設):

```yaml
network_mode: host           # DDS multicast discovery 必需
ipc: shareable / service:X   # SHM transport 跨 container 必需
environment:
  - ROS_DOMAIN_ID=42         # 跟 host 預設 0 隔離,不會誤收到 host 上的 topic
```

下方「常見雷」會解釋這三行為什麼缺一不可。

---

## 🚀 完整 Demo 流程

### Step 1:Build image

```bash
cd /mnt/d/ros_learn/ros2-learning-notes
docker compose -f phase-24-docker/code/docker-capstone/docker-compose.yml build
```

第一次約 3–5 分鐘(下載 ros:humble base、apt install、colcon build)。第二次只要 dockerfile 沒動就走 cache。

### Step 2:啟動兩個 container

```bash
docker compose -f phase-24-docker/code/docker-capstone/docker-compose.yml up -d
```

驗證兩個都跑起來:

```bash
$ docker ps --format 'table {{.Names}}\t{{.Status}}'
NAMES                 STATUS
capstone-lidar        Up 14 seconds
capstone-controller   Up 18 seconds
```

### Step 3:看 controller 的 lifecycle log(驗證過)

```bash
$ docker logs capstone-controller
[entrypoint] ROS_DOMAIN_ID=42
[entrypoint] exec: ros2 launch phase14_capstone capstone.launch.py
[INFO] [launch]: All log files can be found below /root/.ros/log/...
[INFO] [approach_controller-1]: process started with pid [39]
[INFO] [launch.user]: ✅ controller process up, sending configure
[INFO] [approach_controller]: [Capstone] Constructor done (state: unconfigured)
[INFO] [approach_controller]: [Capstone] on_configure
[INFO] [launch.user]: ✅ sending activate
[INFO] [approach_controller]: [Capstone] on_activate
```

### Step 4:跨 container 看 topic / service / action(驗證過)

從 controller container 內或 lidar container 內 source 環境就能用 ros2 CLI:

```bash
$ docker exec capstone-controller bash -lc \
    'source /opt/ros/humble/setup.bash && source /ws/install/setup.bash && \
     ros2 topic list'
/approach_controller/transition_event
/brake_status
/cmd_vel
/lidar_points          ← lidar container 在發
/parameter_events
/rosout
```

**Topic echo `/brake_status`**(controller 發的,reliable)→ 可以看到 `closest_obstacle_distance: 0.4` 證明 controller 有收到 lidar:

```bash
$ docker exec capstone-controller bash -lc '... && ros2 topic echo --once /brake_status'
header:
  stamp: { sec: 1777792471, nanosec: 548257220 }
  frame_id: approach_controller
mode: 1
current_speed: 0.15           ← 因為 0.4m 太近,降到 0.15
closest_obstacle_distance: 0.4
uptime_seconds: 3.0
status_text: '[ENABLED] speed=0.15 obstacle=0.40m'
```

**Service call** 切到 DISABLED 模式:

```bash
$ docker exec capstone-controller bash -lc \
    '... && ros2 service call /set_brake_mode my_robot_interfaces/srv/SetBrakeMode \
            "{mode: 0, max_speed: -1.0, reason: docker-test}"'
requester: making request: SetBrakeMode_Request(mode=0, max_speed=-1.0, reason='docker-test')
response:
SetBrakeMode_Response(success=True, previous_mode=1, applied_max_speed=0.5, message='OK: docker-test')
```

之後 brake_status 會變成 `mode: 0, status_text: '[DISABLED] speed=0.50 obstacle=0.40m'` —— 切成 disabled 後就不再煞車。

**Action send_goal** 從 lidar container 送給 controller container,持續收 feedback:

```bash
$ docker exec capstone-lidar bash -lc \
    '... && timeout 10 ros2 action send_goal /approach \
              my_robot_interfaces/action/Approach \
              "{target_distance: 0.3, approach_speed: 0.5}" --feedback'
Feedback:
  current_distance: 0.4
  elapsed_seconds: 8.4
  status: Approaching
Feedback:
  current_distance: 0.4
  elapsed_seconds: 8.6
  status: Approaching
...
```

**Topic / Service / Action 三種通訊在跨 container 都通了。**

### Step 5:收尾

```bash
docker compose -f phase-24-docker/code/docker-capstone/docker-compose.yml down
```

---

## ☁️ TheConstructSim 對照

雲端 ROSject 本身就是一個 container,**沒辦法在裡面再起 Docker daemon**。所以這章的「Docker 化」對應到雲端的做法是:

| 本章(本機) | TheConstructSim 等價 |
|---------|----------------------|
| `docker compose up` 起兩個 container | 開兩個 ROSject WebShell tab,各跑 controller / lidar |
| `network_mode: host` | ROSject 內所有 process 本來就同 namespace |
| `ROS_DOMAIN_ID=42` 隔離 | ROSject 用獨立 instance,自然隔離 |
| Multi-stage Dockerfile 體積優化 | 不適用(雲端不交付 image) |

**Docker 章只在本機學有意義**——目的是學「怎麼把作品交付給別人 / 部署到實機」。雲端是 ad-hoc 開發環境,不需要 image。

---

## 🐛 常見雷(Docker × ROS 2 五大雷)

### ⚠️ 雷 1:bridge network 下 `topic list` 看得到、`topic echo` 收不到

**症狀**:用預設 bridge network,`docker compose up` 後從 host 或另一個 container 跑 `ros2 topic list` 看得到 `/lidar_points`,但 `ros2 topic echo /lidar_points` 永遠等不到資料。

**原因**:DDS discovery 走 unicast metadata 可以穿透 bridge,但實際 data plane 用 UDP multicast,Docker 預設 bridge 對 multicast 不友善。

**解**:`network_mode: host`。Container 直接用 host 的 network namespace,multicast 暢通。代價是失去 network isolation,但 ROS 系統內部互信本來就需要這個。

**進階解**(多機部署時)**:Phase 20 會教用 FastDDS Discovery Server**,用 unicast peer-to-peer,完全不依賴 multicast。

### ⚠️ 雷 2:host network 解決了,但 sensor data BestEffort 還是收不到

**症狀**:`network_mode: host` 後,reliable topics(`/brake_status`、`/cmd_vel`)能收,但 BestEffort 的 `/lidar_points` 還是收不到。Topic discovery OK,QoS 對得上,就是沒資料。

**原因**:FastRTPS 預設啟用 SHM(shared memory transport)當同 host pub-sub 的快路徑。Docker container 雖然共用 network namespace,**IPC namespace 仍是分開的**——SHM 區段在不同 container 之間打不通,而 fall back 到 loopback UDP 時 BestEffort 不重傳,就直接掉封包。

**解**:`ipc: shareable`(controller)+ `ipc: service:controller`(lidar)。讓兩個 container 共用 IPC namespace,SHM 相通。

```yaml
controller:
  ipc: shareable
lidar:
  ipc: service:controller
```

**驗證 sensor data 真的通了**:`brake_status` 的 `closest_obstacle_distance` 應該等於你 fake_lidar 設的距離(`0.4`),而不是預設的 `100.0`。

### ⚠️ 雷 3:entrypoint 沒寫 `exec "$@"`,docker stop 等 10 秒才殺

**症狀**:`docker stop capstone-controller` 慢吞吞 10 秒才回。

**原因**:entrypoint 是 shell,真正的 ros2 process 是 PID 2。`docker stop` 送 SIGTERM 給 PID 1(shell),shell 沒 forward 給 PID 2,docker 等 stop_grace_period(預設 10s)後 SIGKILL。

**解**:entrypoint 結尾用 `exec "$@"`。`exec` 替換 shell 自己,目標 process 變 PID 1,SIGTERM 直接到。

```bash
# ❌
source /opt/ros/humble/setup.bash
source /ws/install/setup.bash
"$@"

# ✅
source /opt/ros/humble/setup.bash
source /ws/install/setup.bash
exec "$@"
```

### ⚠️ 雷 4:ROS_DOMAIN_ID 沒設,container 跟 host 上的 ros2 互相干擾

**症狀**:Container 起來後,host 上跑 `ros2 topic list` 突然看到 container 內的 topic,或反過來。錄 bag、debug 時會搞混。

**原因**:host network 模式下,container 跟 host 共用 network namespace,DDS discovery 自然互通。如果都用預設 `ROS_DOMAIN_ID=0`,所有 ros2 process 都在同個 domain。

**解**:**容器永遠設不同的 ROS_DOMAIN_ID**(本章用 42)。要更謹慎可以再 export 一個 `RMW_IMPLEMENTATION` 強制特定 DDS 實作。

### ⚠️ 雷 5:Dockerfile 沒 source ROS 就 colcon build,`command not found`

**症狀**:`docker build` 在 `colcon build` 那行炸 `colcon: command not found` 或 `Could not find a package configuration file provided by "rclcpp"`。

**原因**:每個 `RUN` 指令都是新 shell,`/opt/ros/humble/setup.bash` 沒 persist。即使你前面一個 RUN `source` 了,下一個 RUN 還是空環境。

**解**:把 source + colcon 寫在**同一個** RUN,而且要明確用 `bash -c`(`/bin/sh` 不認 `source`):

```dockerfile
# ❌
RUN source /opt/ros/humble/setup.bash
RUN colcon build

# ✅
RUN /bin/bash -c "source /opt/ros/humble/setup.bash && colcon build"
```

### ⚠️ 雷 6 (bonus):`.dockerignore` 沒設,context 傳 1GB 給 daemon

**症狀**:`docker build` 開頭 `Sending build context to Docker daemon` 卡好幾分鐘,最後失敗或超慢。

**原因**:Dockerfile 的 build context 是 repo root(讓我們能讀 sibling phase),但這意味著**整個 repo**——包括 `.git`、build 殘留、舊 archive、所有 phase 的 images——都會傳給 daemon。

**解**:寫 `.dockerignore` 用「全黑名單 + 白名單例外」:

```
*
!phase-08-custom-interfaces/code/my_robot_interfaces/**
!phase-14-capstone-1/code/my_cpp_pkg/**
!phase-24-docker/code/docker-capstone/**
**/build/
**/install/
**/log/
.git/
```

---

## 🎯 學到的關鍵概念

| 概念 | 一句話 |
|------|------|
| Multi-stage build | builder 編完 → runtime 只帶 install/,image 體積小 60–70% |
| `exec "$@"` in entrypoint | 替換 shell 自己,讓目標 process 變 PID 1 收得到 SIGTERM |
| `network_mode: host` | DDS discovery 走 multicast,bridge network 會擋住 |
| `ipc: shareable` | DDS SHM transport 要求同 IPC namespace,bridge 預設不共用 |
| `ROS_DOMAIN_ID` | 同 host 多個 ROS 系統靠這個隔離,不會互相干擾 |
| `.dockerignore` 白名單 | build context 傳 daemon 越小越快,跨 phase 共用根目錄時必設 |
| `docker compose` `build.context` | 設成 repo root 才能讀 sibling phase,但要配合 dockerignore |

---

## 🌟 進階挑戰

1. **體積優化到 <500MB**:把 runtime base 從 `ros:humble-ros-core` 換成 `ubuntu:22.04` + 手動 apt install ros base 必要套件,減少不需要的 ros tools
2. **Healthcheck**:加 `healthcheck:` 自動檢查 controller 的 lifecycle 是否 active,不 active 重啟
3. **加 host volume mount log**:把 `/root/.ros/log` 掛到 host 路徑,容器死了 log 還在
4. **dual-arch image**:用 `docker buildx` build 出 amd64 + arm64,Pi/Jetson 都能用
5. **接 Phase 25 CI**:CI workflow build 完 push 到 GHCR,本地 `docker pull` 取代 `build`

---

## 🔗 下一步

- **[Phase 20 多機通訊](../phase-20-multi-machine/)** — 跨主機(或同主機 bridge network)時用 FastDDS Discovery Server 取代 multicast,Docker 場景直接受益
- **Capstone Final** — 把 Capstone A(SLAM + Nav2)整套 docker compose 化,完成 portfolio 最後一塊

---

## 📁 完整檔案結構

```
phase-24-docker/
├── README.md                      ← 本檔案
├── code/
│   └── docker-capstone/
│       ├── Dockerfile             ← Multi-stage build
│       ├── docker-compose.yml     ← 兩個 service + host network + shared IPC
│       ├── entrypoint.sh          ← source ROS + exec "$@"
│       ├── .dockerignore          ← 全黑名單 + 三條白名單
│       └── scripts/
│           └── fake_lidar.py      ← lidar container 的 entry script
└── images/                        ← (之後補:docker ps / topic echo 截圖)
```
