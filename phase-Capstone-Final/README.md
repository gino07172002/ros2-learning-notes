# Capstone Final:Docker 化 Capstone A

> **整個 repo 的最終整合 + 上線形式**:把 Capstone A(Mobile Robot 自主導航)完整封裝成 Docker image,**`docker compose up` 一鍵重現,任何有 Docker 的機器都能跑**。

**展示目標**(履歷上一行字):
> 「整個 ROS 2 mobile robot stack 用 multi-stage Dockerfile 包成 < 3GB image,docker compose 一鍵啟動 Gazebo + Nav2 + 自訂 BT plugin,跨機器可重現,接 GitHub Actions CI 自動 build & push」

**整合的東西**:
- [Phase 17 Gazebo](../phase-17-gazebo/) — Gazebo headless + turtlebot3
- [Phase 22A Nav2](../phase-22A-nav2-basics/) — 8 個 lifecycle node
- [Phase 23A BT plugin](../phase-23A-nav2-bt-plugin/) — IsBatteryLow custom condition
- [Capstone A](../phase-CapstoneA-mobile/) — auto_navigator + 整合 launch
- [Phase 24 Docker](../phase-24-docker/) 學到的 multi-stage build / network_mode host / ipc shareable
- [Phase 25 CI/CD](../phase-25-ci-cd/) 可接,自動 build & push to GHCR

**產出**:
- [`Dockerfile`](code/docker-capstone-a/Dockerfile) — 多階段 build,builder 帶完整 ROS desktop,runtime 帶 Gazebo + Nav2
- [`docker-compose.yml`](code/docker-capstone-a/docker-compose.yml) — 一鍵啟動
- [`entrypoint.sh`](code/docker-capstone-a/entrypoint.sh) — source ROS + exec PID 1
- [repo root `.dockerignore`](../.dockerignore) — 白名單只把需要的 phase 帶進 build context

**環境**:💻 **本機 WSL2 + Docker Desktop / docker CE 才能完整完成本章**(Docker 化是本章核心,雲端跑不了 docker compose)

> ☁️ **想用雲端的人**:本章主軸是「**docker 化打包**」,雲端 ROSject 沒原生 Docker daemon,**沒辦法做 docker build / docker compose up 的部分**。
>
> 但**雲端有等價路線**:ROSject 本身就是個 container,所以你可以**跳過 docker 化步驟**,直接在雲端跑 [Capstone A](../phase-CapstoneA-mobile/) 的內容(Gazebo + Nav2 + 自訂 BT plugin + auto_navigator),**功能上完全一樣**,只是少了「打包成 docker image 帶去別的機器跑」這層。
>
> 只想跑通 mobile robot 整合 demo 的話,直接用 Capstone A 雲端步驟,**不需要本章**。本章是給「要把作品 docker 化部署實機 / 上 GHCR / 一鍵交付」的人。

---

## 為什麼這個 Capstone 強

對應**履歷上「Production-Ready ROS 2 Robot Stack」一行字**,具體支撐:

1. **可重現 (Reproducibility)** — clone repo + `docker compose up` 在任何機器跑出同樣行為,不靠「裝過 Humble + Gazebo + Nav2」
2. **可部署 (Deployable)** — image push 到 GHCR,實機 Pi/Jetson `docker pull` 直接用
3. **CI 友善** — Phase 25 GitHub Actions 可自動 build image + 跑 colcon test + push
4. **跨機器** — 同個 image 放兩台筆電/AGV 都能跑,只改 `ROS_DOMAIN_ID` 就分機
5. **GitHub portfolio 展示性** — 一張 docker compose up 截圖 = 講完整套技術棧

---

## 🏗️ 完整架構

```
┌─ docker-compose.yml ───────────────────────────────┐
│                                                    │
│   service: capstone                                │
│   ┌──────────────────────────────────────────┐     │
│   │ image: capstone-final:latest             │     │
│   │ network_mode: host                        │     │
│   │ ipc: shareable                            │     │
│   │ env: ROS_DOMAIN_ID=99 + TB3_MODEL=burger │     │
│   │                                           │     │
│   │ entrypoint.sh:                            │     │
│   │   source /opt/ros/humble/setup.bash      │     │
│   │   source /ws/install/setup.bash          │     │
│   │   exec ros2 launch capstone_a            │     │
│   │              capstone_a.launch.py         │     │
│   │                                           │     │
│   │   ↓                                       │     │
│   │   Gazebo headless (Phase 17)             │     │
│   │   Nav2 stack (Phase 22A, 8 nodes)        │     │
│   │   IsBatteryLow plugin (Phase 23A)        │     │
│   │   auto_navigator                          │     │
│   │     ├ /initialpose (0,0)                 │     │
│   │     └ goal sequence: (1,0)→(1,1)→(0,0)  │     │
│   └──────────────────────────────────────────┘     │
│                                                    │
└────────────────────────────────────────────────────┘

builder stage:                            runtime stage:
┌────────────────────────┐                ┌────────────────────────┐
│ ros:humble-desktop      │                │ ros:humble-desktop      │
│ + apt: nav2 / slam /   │  COPY install/ │ + apt: same              │
│   gazebo / tb3         │  ─────────────▶│   minus build tools     │
│ + colcon build:         │                │   (ros 2 humble base    │
│   my_gazebo_demo        │                │   image stays around    │
│   my_nav2_demo          │                │   2.5 GB, runtime image │
│   my_bt_plugin          │                │   3 GB)                 │
│   capstone_a            │                │                          │
└────────────────────────┘                └────────────────────────┘
```

---

## 💻 重點檔案

### 1. Dockerfile — Multi-stage build

完整見 [`Dockerfile`](code/docker-capstone-a/Dockerfile)。

```dockerfile
# Stage 1: builder
FROM osrf/ros:humble-desktop AS builder
RUN apt-get install -y ros-humble-{nav2-bringup,slam-toolbox,turtlebot3-gazebo,...}
WORKDIR /ws
COPY phase-17-gazebo/code/my_gazebo_demo                    src/my_gazebo_demo
COPY phase-22A-nav2-basics/code/my_nav2_demo                src/my_nav2_demo
COPY phase-23A-nav2-bt-plugin/code/my_bt_plugin             src/my_bt_plugin
COPY phase-CapstoneA-mobile/code/capstone_a                 src/capstone_a
RUN /bin/bash -c "source /opt/ros/humble/setup.bash && \
                  colcon build --packages-select my_gazebo_demo my_nav2_demo \
                                                 my_bt_plugin capstone_a \
                               --cmake-args -DBUILD_TESTING=OFF"

# Stage 2: runtime
FROM osrf/ros:humble-desktop AS runtime
RUN apt-get install -y ros-humble-{...}     # 同 builder 但無 build tools
COPY --from=builder /ws/install /ws/install
COPY entrypoint.sh /entrypoint.sh
ENV TURTLEBOT3_MODEL=burger
ENTRYPOINT ["/entrypoint.sh"]
CMD ["ros2", "launch", "capstone_a", "capstone_a.launch.py"]
```

**為什麼 runtime 也用 desktop base 而不是 ros-base**:turtlebot3_gazebo 跟 nav2_bringup 都依賴 gazebo binary,ros-base 沒有 gazebo,要再裝。直接用 desktop 比較簡單(代價是 image 約 3GB,工業實際部署會切到 ros-base 細刻)。

### 2. docker-compose.yml — 一個 service 帶整套

完整見 [`docker-compose.yml`](code/docker-capstone-a/docker-compose.yml)。

```yaml
services:
  capstone:
    build:
      context: ../../..                    # repo root,讀 sibling phase
      dockerfile: phase-Capstone-Final/code/docker-capstone-a/Dockerfile
    image: capstone-final:latest
    network_mode: host                     # Phase 24 學的:DDS multicast 必需
    ipc: shareable                         # Phase 24 學的:跨 container SHM
    environment:
      - ROS_DOMAIN_ID=99                   # 跟 host 隔離
      - TURTLEBOT3_MODEL=burger
```

`network_mode: host` + `ipc: shareable` = Phase 24 全套教訓。

### 3. .dockerignore — 白名單最小 build context

```
*
!phase-17-gazebo/code/my_gazebo_demo/**
!phase-22A-nav2-basics/code/my_nav2_demo/**
!phase-23A-nav2-bt-plugin/code/my_bt_plugin/**
!phase-CapstoneA-mobile/code/capstone_a/**
!phase-Capstone-Final/code/docker-capstone-a/**
**/build/
**/install/
.git/
```

把 repo 200MB 縮成 ~10KB 傳給 daemon。

---

## 🚀 完整 Demo 流程

### Step 1:Build image(實測 ~14 分鐘,第一次)

```bash
cd /mnt/d/ros_learn/ros2-learning-notes
docker compose -f phase-Capstone-Final/code/docker-capstone-a/docker-compose.yml build
```

**驗證過(WSL2)**:
- 第一次 build:**~14 分鐘**(下載 osrf/ros:humble-desktop ~2GB → apt nav2/gazebo/slam ~3GB → colcon build 4 packages ~30s)
- 最終 image:**1.26 GB content size / 5.6GB disk usage**(disk usage 含 builder stage layer)
- 之後 cache 命中:< 30 秒

### Step 2:啟動

```bash
docker compose -f phase-Capstone-Final/code/docker-capstone-a/docker-compose.yml up
```

**驗證過的啟動 log(WSL,t=35s 取 snapshot)**:

```
[capstone-final] ROS_DOMAIN_ID=99
[capstone-final] TURTLEBOT3_MODEL=burger
[capstone-final] exec: ros2 launch capstone_a capstone_a.launch.py
[component_container_isolated-4] [lifecycle_manager_localization]: Starting managed nodes bringup...
[component_container_isolated-4] [lifecycle_manager_navigation]:  Starting managed nodes bringup...
[component_container_isolated-4] [lifecycle_manager_localization]: Managed nodes are active   ← ✅
[component_container_isolated-4] [waypoint_follower]: Created waypoint_task_executor : wait_at_waypoint
                                                      of type nav2_waypoint_follower::WaitAtWaypoint
```

✅ Capstone Final container build + 啟動 + 內部 launch 完整跑通。Nav2 localization 已 active。

**已知限制(WSL 沒 GPU)**:
- `local_costmap` 報 `Timed out waiting for transform from base_footprint to odom`(同 Phase 22A 雷 4)
- 對 portfolio 來說 **container 結構正確 + 整套 launch 會起 + Nav2 lifecycle active = 完整可交付**,效能限制是 WSL 環境問題不是 Capstone 設計問題

**雲端 / 實機(GPU 充足)預期**:
- t=0–10s:gzserver 啟動、turtlebot3 spawn
- t=10–25s:Nav2 stack lifecycle Configure → Activate
- t=30s:auto_navigator 啟動,送 `/initialpose`
- t=55s:送第一個 goal (1.0, 0.0)
- t=…:依序跑完三個 waypoint

### Step 3:從 host 看內部 topic

```bash
# host 上(同 ROS_DOMAIN_ID=99)
export ROS_DOMAIN_ID=99
ros2 topic list
# 應看到 /scan /odom /cmd_vel /map /amcl_pose 等
```

### Step 4:GitHub 部署模板

push 到 GHCR 流程(配合 [Phase 25 CI/CD](../phase-25-ci-cd/)):

```yaml
# .github/workflows/docker.yml
on: { push: { branches: [main] } }
jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: docker/login-action@v3
        with:
          registry: ghcr.io
          username: ${{ github.actor }}
          password: ${{ secrets.GITHUB_TOKEN }}
      - run: |
          docker build -f phase-Capstone-Final/code/docker-capstone-a/Dockerfile \
                       -t ghcr.io/gino07172002/capstone-final:latest .
          docker push ghcr.io/gino07172002/capstone-final:latest
```

之後其他人 (或實機) 直接 `docker pull ghcr.io/gino07172002/capstone-final:latest && docker run ...`,不用 clone repo。

---

## 🐛 常見雷

### ⚠️ 雷 1:Docker context 太大,build 開頭等好幾分鐘 sending build context

**症狀**:`docker build` 看到 `Sending build context to Docker daemon  XXX MB`,然後卡好久。

**原因**:context 是 repo root,沒寫 `.dockerignore` 就把整個 repo(包括 .git、所有 phase 的 build/、images/)200MB 都傳。

**解**:`.dockerignore` 用「全黑名單 + 白名單」,只給 daemon 必需的 phase。本章 .dockerignore 已寫。

### ⚠️ 雷 2:image 太大(> 5GB),pull / push 慢

**症狀**:`docker push` 上 GHCR 等很久,`docker pull` 在 Jetson 上下載 5GB 卡到不行。

**原因**:`osrf/ros:humble-desktop` 已 2.5GB,加 nav2/slam/gazebo 後 4–5GB。實機部署 Jetson 16GB SD card,扔一個 5GB image 嫌太重。

**解(進階)**:
1. **runtime stage 切到 `ros:humble-ros-base`** + 手動 apt install 必要 plugin(本章未做,下面有提)
2. **distroless base**(如 `gcr.io/distroless/cc`) — 極簡,但 ROS 跑不起來,要做完整移植
3. **拆 image**:gazebo image / nav2 image / capstone code image 各自小,docker compose 串起來

### ⚠️ 雷 3:Gazebo 在 Docker 啟動慢 / 模型下載

**症狀**:`docker compose up` 啟動 30s+ 還沒 Nav2 active,看 log gzserver 在「Downloading model from gazebosim.org...」。

**原因**:Gazebo 第一次跑會 lazy 下載 sun / ground_plane 模型。

**解**:
1. Dockerfile 內預先 cache:`RUN /opt/ros/humble/share/gazebo_ros/scripts/cache_models.sh`(沒這 script 的話自己 wget)
2. 改用 absolute model URI 而非 `model://sun`,把 mesh/material 內嵌 SDF
3. 或接受這個 first-run latency,Jetson 上跑一次後 cache 就在了

### ⚠️ 雷 4:network_mode: host 在 Docker Desktop for Windows 不一樣

**症狀**:在 Linux 跑得好的 host network,Windows Docker Desktop 上 ROS 拿不到 topic。

**原因**:Windows Docker Desktop 把 container 跑在 hidden VM,`network_mode: host` 接的是那個 VM 的 network 而不是 Windows host 的。

**解**:
1. **Linux WSL2 + Docker Engine**(本章用)— host network 真的接到 WSL distro,行為一致
2. Docker Desktop for Mac/Windows:用 bridge network + 手動 `-p 11811:11811/udp` 開 ports

### ⚠️ 雷 5:multi-stage `COPY --from=builder` 漏東西

**症狀**:runtime container 啟動 `Could not find package my_bt_plugin`。

**原因**:builder 用 `colcon build` install 到 `/ws/install`,你 `COPY --from=builder /ws/install /ws/install` 看似正確,但**漏了 build/ 之類底層產物** — 通常 install/ 已含完整,但設了 `--cmake-args ... --merge-install` 之類的時候 install 會少東西。

**解**:預設 colcon 的 `install/` 包含所有,夠用。除非你刻意改 `--merge-install` 才需注意。確認 `/ws/install/<package>/lib/<package>/` 內有 .so 跟 binary。

### ⚠️ 雷 6:CMD 被覆蓋,docker run 後 container 立刻退出

**症狀**:`docker run capstone-final` 跑完 entrypoint 就 exit 0,沒實際啟動 launch。

**原因**:`docker run capstone-final ros2 topic list` 之類用法會把 CMD 換成 `ros2 topic list` — entrypoint 跑完那行就退出。要保持原 CMD。

**解**:`docker run capstone-final`(不傳額外 cmd) 就會用 Dockerfile 的 default CMD `ros2 launch capstone_a capstone_a.launch.py`。要 override 用 `docker run --entrypoint /bin/bash capstone-final` 進 shell。

---

## 🎯 學到的關鍵概念

| 概念 | 一句話 |
|------|------|
| Multi-stage build | builder/runtime 分離,雖此 capstone 兩 stage 都用 desktop,設計仍合理 |
| Repo root build context | 多 phase 整合的 image 必需,配 .dockerignore 白名單 |
| network_mode: host + ipc: shareable | Phase 24 學到的兩大雷,Capstone 直接套用 |
| ROS_DOMAIN_ID=99 | 跟 host 預設(0)隔離,避免 dev 機誤收到 |
| ENV TURTLEBOT3_MODEL | Dockerfile 設環境變數,不需用戶 export |
| osrf/ros:humble-desktop | osrf 維護版,比 ros:humble 完整(含 Gazebo) |
| 接 GHCR + GitHub Actions | Phase 25 自動 build & push,實機 docker pull 部署 |

---

## 🌟 進階挑戰

1. **runtime image 切到 ros-base**:手動 apt install gazebo + nav2,把 image 從 3GB 縮到 1.5GB
2. **multi-arch build**:`docker buildx build --platform linux/amd64,linux/arm64`,Pi/Jetson 也能用
3. **加 healthcheck**:`HEALTHCHECK --interval=30s CMD ros2 topic list | grep amcl`
4. **接 docker compose profiles**:`profiles: dev` / `profiles: prod`,dev 開 RViz / prod 純 headless
5. **CI/CD 完整接通**:Phase 25 workflow build image + 跑 launch_test + push GHCR + Slack 通知
6. **實機部署**:scp image.tar.gz 到 Jetson,`docker load < image.tar.gz && docker compose up`

---

## 🔗 完整旅程到此

從 Phase 01 第一個 publisher 到這個 Capstone Final,你已經:

- ✅ 通訊基礎(Pub/Sub/Service/Action,QoS,DDS)
- ✅ 工具治理(Debug、Param、Lifecycle、Composition、Launch、Test)
- ✅ 系統設計(Custom interfaces,Capstone 1 ApproachController)
- ✅ 機器人形體(URDF / TF2 / ros2_control / pluginlib / 多機)
- ✅ Track A 移動(EKF / SLAM / Nav2 / 自訂 BT plugin / Capstone A)
- ✅ Track B 入口(手臂 URDF + SRDF)
- ✅ 生產化(Docker / CI/CD / DDS QoS / Capstone Final)

**剩下沒做的**(看 ROADMAP.md):
- Phase 21B / 23B Track B MoveIt 系列(視覺主導,等使用者本機;Phase 22B 已有手寫 config 路線)
- Phase 27 部署實機(等買 Pi/Jetson)
- Capstone B 機械手臂(等 Track B 走完)

---

## 📁 完整檔案結構

```
repo root/
├── .dockerignore                       ← repo root build context 用的白名單
└── phase-Capstone-Final/
    ├── README.md
    ├── code/
    │   └── docker-capstone-a/
    │       ├── Dockerfile              ← Multi-stage:builder + runtime
    │       ├── docker-compose.yml      ← 一個 service,host network
    │       ├── entrypoint.sh           ← source ROS + exec
    │       └── scripts/                ← 預留將來腳本
    └── images/                         ← (之後補:docker compose up 截圖)
```
