# Phase 20:多機通訊 — ROS_DOMAIN_ID + FastDDS Discovery Server

> 兩個應用問題:**(a) 同一 host 多個 ROS 系統怎麼不互相干擾**?**(b) Multicast 被擋住的網路怎麼讓 Node 之間能找到彼此**?

**學完你會**:
- 用 `ROS_DOMAIN_ID` 在同一 host 隔離多個 ROS 2 系統,**完全不需要改 code**
- 知道 ROS 2 預設 discovery 為什麼依賴 multicast、什麼網路會擋它
- 用 **FastDDS Discovery Server** 取代 multicast,實作 server-client 形式的 unicast discovery
- 用 docker compose 模擬「兩台機器」做端到端驗證,不需要真的有兩台機器

**前置**:
- [Phase 24 Docker](../phase-24-docker/) — 我們重用 Docker 做兩個 demo 的「假機器」
- [Phase 02 通訊概念](../phase-02-communication-concepts/) — 已介紹 DDS / discovery 觀念
- 對 UDP unicast vs multicast、bridge vs host network 有概念

**產出**:
- [`code/Dockerfile`](code/Dockerfile) — `ros:humble-ros-base` + `demo_nodes_cpp` + `fastdds` 工具的 image
- [`code/domain-id-demo/`](code/domain-id-demo/) — Demo 1:同 host 不同 domain 的隔離
- [`code/discovery-server/`](code/discovery-server/) — Demo 2:Discovery Server 取代 multicast

**環境**:💻 本機 WSL2(Docker Desktop 或 docker CE)
> ☁️ TheConstructSim:可跑 ROS_DOMAIN_ID 部分(直接開兩個 WebShell tab 設不同 ID),但雲端 ROSject 內部 Docker daemon 不可用,Discovery Server 部分等本機跑。

---

## 為什麼這章重要

部署到實機之後你**一定**會遇到的兩個情境:

1. **同台機器跑多個 ROS 系統**:筆電同時 debug 兩台機器人(各自的 simulator),或者 CI 流水線在一台 host 跑多個並行測試。沒有 `ROS_DOMAIN_ID` 隔離 → 所有節點都看得到彼此 → 訊息亂飛、test 互相干擾。

2. **Multicast 被擋住的網路**:Discovery 預設用 UDP multicast,但這在很多場合會被擋——
   - **企業 / 校園 WiFi**:大多禁 multicast 防 mDNS / Bonjour
   - **AWS / GCP VPC**:預設不轉發 multicast
   - **Docker bridge network**:跨 container multicast 不可靠
   - **多個 VLAN**:預設不跨 VLAN 轉發

兩個問題都不需要改 ROS code,只要懂環境變數 + 一個 broker 工具。

---

## 🏗️ 兩個 Demo 架構

### Demo 1:Domain ID 隔離(同 host)

```
       host network + IPC shareable
┌────────────────────────────────────────────┐
│                                            │
│   talker-d11    listener-d11   listener-d99 │
│   ROS_DOMAIN=11 ROS_DOMAIN=11  ROS_DOMAIN=99│
│        │            ▲                ▲     │
│        └─ DDS ──────┘                X     │
│                              (隔離,收不到) │
└────────────────────────────────────────────┘
```

兩個 listener 用「同樣的 image、同樣的網路、同樣的 IPC」——**唯一差異是 ROS_DOMAIN_ID**。
同 ID 的看得到,不同 ID 的完全看不到。

### Demo 2:Discovery Server 取代 multicast

```
         host network
┌────────────────────────────────────────┐
│                                        │
│         ┌────────────────────┐         │
│         │  fastdds discovery │         │
│         │   server (broker)  │         │
│         │  127.0.0.1:11811   │         │
│         └─────┬────────┬─────┘         │
│  unicast ↑    │        │   ↑ unicast   │
│         ┌─┴───┘        └───┴─┐         │
│         │ talker      listener│        │
│         │ DOMAIN=42   DOMAIN=42│       │
│         │ DISC_SVR=   DISC_SVR=│       │
│         │  127.0.0.1: 127.0.0.1:│      │
│         │  11811      11811    │       │
│         └─────────────────────┘        │
│                                        │
│  完全不用 multicast — 全 unicast       │
└────────────────────────────────────────┘
```

每個 client 不再用 multicast 廣播尋找彼此,而是 **點對點 unicast 跟 server 註冊**。
Server 替它們轉發 metadata,client 知道對方存在後實際 data plane 還是 client-to-client unicast。

---

## 💻 重點檔案

### Dockerfile — 比 Phase 24 輕量的 image

完整見 [`code/Dockerfile`](code/Dockerfile)。

```dockerfile
FROM ros:humble-ros-base                 # 比 ros-core 多 demo_nodes_cpp 等東西
RUN apt-get update && apt-get install -y --no-install-recommends \
        ros-humble-demo-nodes-cpp \
        ros-humble-rmw-fastrtps-cpp \
    && rm -rf /var/lib/apt/lists/*
# entrypoint:source ROS + 印出 DOMAIN_ID / DISCOVERY_SERVER 方便 debug + exec "$@"
```

為什麼 base 從 `ros-core` 升到 `ros-base`:Phase 24 為了 image 小用 ros-core,但這章要 demo `demo_nodes_cpp`(talker / listener)以及 `fastdds` 工具,ros-core 都沒有。

### Demo 1 docker-compose.yml — 三 service 共用同 image

完整見 [`code/domain-id-demo/docker-compose.yml`](code/domain-id-demo/docker-compose.yml)。

關鍵 3 行:

```yaml
talker-d11:    { environment: { ROS_DOMAIN_ID: 11 }, command: ros2 run demo_nodes_cpp talker }
listener-d11:  { environment: { ROS_DOMAIN_ID: 11 }, command: ros2 run demo_nodes_cpp listener }
listener-d99:  { environment: { ROS_DOMAIN_ID: 99 }, command: ros2 run demo_nodes_cpp listener }
```

三者都 `network_mode: host` + `ipc: service:talker-d11`(沿用 Phase 24 的雷修法),
**唯一變數就是 DOMAIN_ID**。

### Demo 2 docker-compose.yml — Discovery Server + 兩 client

完整見 [`code/discovery-server/docker-compose.yml`](code/discovery-server/docker-compose.yml)。

關鍵環境變數:

```yaml
discovery:
  command: fastdds discovery -i 0 -l 127.0.0.1 -p 11811   # 監聽 UDP 11811

talker / listener:
  environment:
    - ROS_DOMAIN_ID=42
    - ROS_DISCOVERY_SERVER=127.0.0.1:11811                # 點到 server,不靠 multicast
    - RMW_IMPLEMENTATION=rmw_fastrtps_cpp                  # 必須 fastrtps
```

ROS 2 Humble 看到 `ROS_DISCOVERY_SERVER` 環境變數會**自動把 client 設成 super-client**(會主動接收 server 推送的 endpoints,而不是只查詢)。所以不用額外的 fastdds XML profile。

---

## 🚀 完整 Demo 流程

### 前置:Build phase20 image(只一次)

```bash
cd /mnt/d/ros_learn/ros2-learning-notes
docker compose -f phase-20-multi-machine/code/domain-id-demo/docker-compose.yml build
```

(Demo 2 也用同一個 image,不用重 build)

### Demo 1:Domain ID 隔離

```bash
docker compose -f phase-20-multi-machine/code/domain-id-demo/docker-compose.yml up -d
sleep 8
docker logs p20-talker-d11      | tail -5
docker logs p20-listener-d11    | tail -5
docker logs p20-listener-d99    | tail -5
```

**驗證過的輸出**:

```
=== TALKER (DOMAIN_ID=11) ===
[INFO] [talker]: Publishing: 'Hello World: 11'

=== LISTENER D11 (DOMAIN_ID=11) ===
[INFO] [listener]: I heard: [Hello World: 11]      ← ✅ 同 domain,收到了

=== LISTENER D99 (DOMAIN_ID=99) ===
[entrypoint] ROS_DOMAIN_ID=99
[entrypoint] exec: ros2 run demo_nodes_cpp listener
                                                    ← ❌ 啥都沒收到,純隔離
```

收尾:

```bash
docker compose -f phase-20-multi-machine/code/domain-id-demo/docker-compose.yml down
```

### Demo 2:Discovery Server

```bash
docker compose -f phase-20-multi-machine/code/discovery-server/docker-compose.yml up -d
sleep 8
docker logs p20-discovery   | tail -8
docker logs p20-talker      | tail -5
docker logs p20-listener    | tail -5
```

**驗證過的輸出**:

```
=== DISCOVERY SERVER ===
### Server is running ###
  Participant Type:   SERVER
  Server ID:          0
  Server GUID prefix: 44.53.00.5f.45.50.52.4f.53.49.4d.41
  Server Addresses:   UDPv4:[127.0.0.1]:11811

=== TALKER ===
[INFO] [talker]: Publishing: 'Hello World: 8'

=== LISTENER ===
[INFO] [listener]: I heard: [Hello World: 8]        ← ✅ 透過 server 完成 discovery
```

收尾:

```bash
docker compose -f phase-20-multi-machine/code/discovery-server/docker-compose.yml down
```

---

## ☁️ TheConstructSim 對照

### Demo 1(Domain ID)在雲端怎麼玩

ROSject 開兩個 WebShell tab,分別:

```bash
# Tab A
export ROS_DOMAIN_ID=11
ros2 run demo_nodes_cpp talker

# Tab B
export ROS_DOMAIN_ID=11
ros2 run demo_nodes_cpp listener     # 收得到

# Tab C
export ROS_DOMAIN_ID=99
ros2 run demo_nodes_cpp listener     # 收不到
```

完全等價於 Demo 1。

### Demo 2(Discovery Server)在雲端

ROSject 內 Docker daemon 不可用,但**直接在 WebShell 跑 fastdds 工具就行**:

```bash
# Tab A:server
fastdds discovery -i 0 -l 127.0.0.1 -p 11811

# Tab B:talker
export ROS_DISCOVERY_SERVER=127.0.0.1:11811
ros2 run demo_nodes_cpp talker

# Tab C:listener
export ROS_DISCOVERY_SERVER=127.0.0.1:11811
ros2 run demo_nodes_cpp listener
```

效果一樣,只是少了「兩台不同機」的視覺感。本機 docker 版才能展示「跨網段 discovery」的工程價值。

---

## 🐛 常見雷

### ⚠️ 雷 1:設了 `ROS_DOMAIN_ID` 但仍互通

**症狀**:export 了 `ROS_DOMAIN_ID=11` 結果還是看到 host 上其他 ROS 系統的 topic。

**原因**:
1. `ros2 daemon` 仍是舊 domain 的 cache
2. shell 沒重 source(forgot 在新 terminal export)

**解**:
```bash
ros2 daemon stop                      # 殺 daemon
export ROS_DOMAIN_ID=11               # 在每個 terminal 都要設
ros2 topic list --no-daemon           # 第一次跑加 --no-daemon 避免 cache 干擾
```

### ⚠️ 雷 2:Domain ID 範圍

**症狀**:設 `ROS_DOMAIN_ID=232` 在某些 host 突然啟動失敗 / 收不到任何訊息。

**原因**:DDS 用 domain id 計算 UDP port(`base + 250 * domain_id`)。**0–101 是安全範圍**,>101 容易撞到系統其他服務的 port,>232 會超過 65535 而完全無法用。

**解**:同 host 不同 ROS 系統,用 0–101 即可,例如 11、22、42。

### ⚠️ 雷 3:Discovery Server 開 bridge network 客戶端註冊不上去

**症狀**:`fastdds discovery` server 起來了,server log 看 "Server is running",但 client(talker / listener)的 `ros2 topic list` 永遠看不到對方的 topic。

**原因**:Docker 預設 bridge network + Humble FastRTPS 2.6.11 的組合下,client 雖然 unicast 連得到 server IP,但 server 替 client 轉發的 endpoints metadata 似乎沒順利回傳。具體成因深入到 fastrtps 內部,WSL 加上 Docker Desktop 的網路 stack 又有額外複雜度。

**解(本章採用)**:**全部走 host network**(`network_mode: host` + `ROS_DISCOVERY_SERVER=127.0.0.1:11811`)。雖然 host network 下 multicast 本來也通,但用 Discovery Server 是為了**模擬「未來同一個 compose 跨真實機器部署」的設定方式**——把 server IP 換成實機 IP 就直接能用。

**進階**:如果一定要 bridge network,需要寫 fastdds XML profile 設定 super-client + 明確列 server 的 IP,並設 `FASTRTPS_DEFAULT_PROFILES_FILE`。範例 XML 見 [eProsima 文件](https://fast-dds.docs.eprosima.com/en/latest/fastdds/discovery/discovery_server.html)。

### ⚠️ 雷 4:`RMW_IMPLEMENTATION` 沒設成 fastrtps

**症狀**:client 設了 `ROS_DISCOVERY_SERVER=...` 但完全沒效果,client 行為跟沒設一樣(走 multicast)。

**原因**:**只有 FastRTPS 支援 Discovery Server**,Cyclone DDS 不支援。如果你的 system 預設 RMW 是 Cyclone(部分 distro 預設),`ROS_DISCOVERY_SERVER` 會被無聲忽略。

**解**:強制設 `RMW_IMPLEMENTATION=rmw_fastrtps_cpp`,並確認 `ros-humble-rmw-fastrtps-cpp` 套件有裝。

### ⚠️ 雷 5:`/tmp` 在 WSL 會被 cleanup

**症狀**:用 `setsid ... > /tmp/foo.log 2>&1 &` 跑 background talker,過幾分鐘 log 檔不見了 / process 也消失了。

**原因**:WSL2 的 systemd cleanup task 會清 `/tmp`,而且 setsid 出去的 child 在某些情況跟 wsl 命令的「會話」綁在一起,wsl 命令結束就被收掉。

**解**:
1. log 寫到 `~/p20_logs/` 之類的家目錄,不是 `/tmp`
2. **放棄「同 host 兩個 terminal 跑 talker」這種驗證,直接用 docker container — daemon 永續、log 永續、行為可預測**

這也是本章直接採用 Docker 模擬「兩台機器」的根本原因。

### ⚠️ 雷 6:Discovery Server 只設一邊

**症狀**:talker 設了 `ROS_DISCOVERY_SERVER`,listener 沒設 → 兩邊看不到彼此。

**原因**:Discovery Server 是「**所有要互通的 client 都得指到同一個 server**」。只有 talker 設,等於 talker 跟 server 講話,但 listener 還在 multicast 模式找不到 server 也找不到 talker。

**解**:任何要加入這個 ROS 系統的 process,都要設一樣的 `ROS_DISCOVERY_SERVER`。可以放 `~/.bashrc` 或 systemd unit 環境變數,實機部署時統一設定。

---

## 🎯 學到的關鍵概念

| 概念 | 一句話 |
|------|------|
| `ROS_DOMAIN_ID` | UDP port offset,改它等於完全換一張 DDS 網路 |
| Discovery Server | server-client 模式取代 multicast,適合 multicast 被擋的網路 |
| Super-Client | 主動接收 server 推送 endpoints 的 client,Humble 有 `ROS_DISCOVERY_SERVER` 自動啟用 |
| `RMW_IMPLEMENTATION` | Discovery Server 只在 FastRTPS 工作,Cyclone DDS 沒這 feature |
| Docker compose 當「假多機」 | 比真實兩台機更可控,適合教學跟 CI |

**業界使用場景**:
- 工廠機器人接公司 WiFi(擋 multicast)→ 中央伺服器跑 Discovery Server
- AWS 上跑分散式 ROS 系統 → ECS task 各自設 `ROS_DISCOVERY_SERVER` 指向 ELB 後的 broker
- CI 平行跑多個機器人測試 → 每個 job 自己 random 一個 `ROS_DOMAIN_ID`

---

## 🌟 進階挑戰

1. **多 server 容錯**:`ROS_DISCOVERY_SERVER=ip1:11811;ip2:11811` 可指多個 server,任一活著就能 discovery。試殺一個看會不會 failover
2. **跨真實機器**:把 listener 容器 image push 到 GHCR,在第二台 PC 用 `docker run --network host -e ROS_DISCOVERY_SERVER=<第一台 IP>:11811 ...` 真的跨機跑
3. **TCP 模式**:fastdds discovery 也支援 TCP(`-t / -q`),適合穿越 NAT 或一些只開 TCP port 的 firewall
4. **回頭重跑 Phase 24**:把 Phase 24 的 Capstone 1 改成跨容器靠 Discovery Server,移除 `network_mode: host` 的依賴

---

## 🔗 下一步

- **[Phase 26 DDS QoS](../phase-26-dds-qos/)** — 連線後的訊息可靠性 / durability,跟 discovery 是不同層次的問題
- **[Phase 24 Docker](../phase-24-docker/)** — 回頭看 host network + IPC 的雷,跟本章 Discovery Server 一起構成「多容器 ROS 系統」的完整工具箱
- **Capstone Final** — 把 Capstone A 跨多 service 部署,Discovery Server 取代 multicast

---

## 📁 完整檔案結構

```
phase-20-multi-machine/
├── README.md                              ← 本檔案
├── code/
│   ├── Dockerfile                         ← ros-base + demo_nodes + fastrtps
│   ├── domain-id-demo/
│   │   └── docker-compose.yml             ← Demo 1:三 service 不同 DOMAIN_ID
│   └── discovery-server/
│       └── docker-compose.yml             ← Demo 2:server + talker + listener
└── images/                                ← (之後補:logs 截圖)
```
