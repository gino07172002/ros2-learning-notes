# Phase 20:多機通訊 — ROS_DOMAIN_ID + Fast DDS Discovery Server

> 這章處理兩個部署時很常見的問題:**同一台 host 上有多個 ROS 系統時,怎麼避免彼此干擾**?以及**網路擋 multicast 時,不同機器上的 node 怎麼找到彼此**?

**學完你會**:
- 用 `ROS_DOMAIN_ID` 在同一台 host 隔離多個 ROS 2 系統,而且**完全不用改 code**
- 看懂 ROS 2 discovery 為什麼依賴 multicast,以及哪些網路環境會擋它
- 用 **Fast DDS Discovery Server** 取代 multicast,讓 discovery 改走 server-client 形式的 unicast
- 用 Docker Compose 模擬「多台機器」做端到端驗證,不需要真的準備兩台電腦

**前置**:
- [Phase 24 Docker](../phase-24-docker/) — 這章重用 Docker 來做可控的「假多機」環境
- [Phase 02 通訊概念](../phase-02-communication-concepts/) — 已介紹 DDS / discovery 的基本概念
- 對 UDP unicast vs multicast、bridge vs host network 有概念

**產出**:
- [`code/Dockerfile`](code/Dockerfile) — `ros:humble-ros-base` + `demo_nodes_cpp` + `fastdds` 工具的 image
- [`code/domain-id-demo/`](code/domain-id-demo/) — Demo 1:同一 host 上用不同 domain 隔離系統
- [`code/discovery-server/`](code/discovery-server/) — Demo 2:Discovery Server 取代 multicast

**環境**:💻 本機 WSL2(Docker Desktop 或 docker CE)
> ☁️ TheConstructSim:可以練 `ROS_DOMAIN_ID` 的部分,直接開多個 WebShell tab 設不同 ID。雲端 ROSject 通常不能用內部 Docker daemon,Discovery Server demo 建議留到本機 WSL2 跑。

---

## 為什麼這章重要

ROS 2 在自己的電腦上跑起來通常很單純,但一到部署、測試或多人共用網路,discovery 就會變成第一個不穩定來源。這章先把兩個最常見的情境拆開處理:

1. **同台機器跑多個 ROS 系統**:例如筆電同時 debug 兩台機器人,或 CI 在同一台 host 平行跑多組測試。沒有 `ROS_DOMAIN_ID` 隔離時,所有節點都看得到彼此,訊息會互相污染,測試也會互相干擾。

2. **Multicast 被擋住的網路**:ROS 2 預設靠 UDP multicast 做 discovery,但 multicast 在很多環境都不可靠:
   - **企業 / 校園 WiFi**:大多禁 multicast 防 mDNS / Bonjour
   - **AWS / GCP VPC**:預設不轉發 multicast
   - **Docker bridge network**:跨 container multicast 不可靠
   - **多個 VLAN**:預設不跨 VLAN 轉發

好消息是,這兩個問題通常都不用改 ROS code。你只需要掌握幾個環境變數,再加上一個 discovery server 工具。

---

## 🏗️ 兩個 Demo 架構

### Demo 1:Domain ID 隔離(同一台 host)

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

三個 container 使用同一個 image、同一種 host network、同一組 IPC 設定。**唯一差異只有 `ROS_DOMAIN_ID`**。

結果很乾淨:同 ID 的 talker/listener 會互相看到;不同 ID 的 listener 會被隔在另一個 ROS graph,完全收不到 topic。

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

Discovery Server 的角色不是轉發 ROS topic data,而是幫 node 交換「誰在這個 ROS graph 裡」的 metadata。

每個 client 不再用 multicast 到處廣播,而是用 unicast 向 server 註冊。等 server 幫它們交換完 endpoint 資訊後,實際 topic data 仍然是 client-to-client 傳輸。

---

## 🕵️ 終端機偵探課:先看 DDS graph 有沒有隔離

寫多機設定前,先學會用終端機判斷「節點是真的沒起來」,還是「起來了但 discovery 看不到」。這章最常見的錯不是 C++ code,而是 DDS domain、RMW、Discovery Server 環境變數沒對上。

### 偵探 1:Domain ID 是否真的隔離

先 build image,再啟動 Demo 1。啟動後先不要看 compose 檔,直接看 container 的 log:

```bash
docker compose -f phase-20-multi-machine/code/domain-id-demo/docker-compose.yml build
docker compose -f phase-20-multi-machine/code/domain-id-demo/docker-compose.yml up -d
sleep 8

docker logs p20-listener-d11 | tail -3
docker logs p20-listener-d99 | tail -3
```

預期看到:

```text
p20-listener-d11  有 I heard: [Hello World: ...]
p20-listener-d99  只有 entrypoint,沒有 I heard
```

這代表 talker 和 `listener-d11` 在同一個 ROS graph 裡;`listener-d99` 因為 `ROS_DOMAIN_ID` 不同,被隔離在另一個 graph。

再確認環境變數:

```bash
docker exec p20-listener-d11 printenv ROS_DOMAIN_ID
docker exec p20-listener-d99 printenv ROS_DOMAIN_ID
```

預期分別是 `11` 和 `99`。如果兩邊都收到訊息,第一件事就是查這裡。

### 偵探 2:Discovery Server 是否真的被 client 使用

啟動 Demo 2 後,先看 server 是否有起來,再看 client 是否指到它:

```bash
docker compose -f phase-20-multi-machine/code/discovery-server/docker-compose.yml up -d
sleep 8

docker logs p20-discovery | grep -E "Server is running|Server Addresses"
docker exec p20-talker printenv ROS_DISCOVERY_SERVER RMW_IMPLEMENTATION
docker exec p20-listener printenv ROS_DISCOVERY_SERVER RMW_IMPLEMENTATION
```

預期看到:

```text
### Server is running ###
Server Addresses: UDPv4:[127.0.0.1]:11811
127.0.0.1:11811
rmw_fastrtps_cpp
```

**這章要做的事**:不是改 ROS node,而是學會用環境變數改變 DDS discovery 行為。只要 domain、RMW、Discovery Server 三件事對齊,talker/listener 的 code 完全不用動。

---

## 💻 重點檔案

### Dockerfile — demo 專用的 ROS base image

完整見 [`code/Dockerfile`](code/Dockerfile)。

```dockerfile
FROM ros:humble-ros-base                 # 比 ros-core 多 demo_nodes_cpp 等東西
RUN apt-get update && apt-get install -y --no-install-recommends \
        ros-humble-demo-nodes-cpp \
        ros-humble-rmw-fastrtps-cpp \
    && rm -rf /var/lib/apt/lists/*
# entrypoint:source ROS + 印出 DOMAIN_ID / DISCOVERY_SERVER 方便 debug + exec "$@"
```

Phase 24 為了縮小 image 用 `ros-core`,但這章需要 `demo_nodes_cpp` 的 talker/listener,也需要 `fastdds` 工具。這些不在 `ros-core` 裡,所以這裡改用 `ros-base`。

### Demo 1 docker-compose.yml — 三 service 共用同 image

完整見 [`code/domain-id-demo/docker-compose.yml`](code/domain-id-demo/docker-compose.yml)。

關鍵 3 行:

```yaml
talker-d11:    { environment: { ROS_DOMAIN_ID: 11 }, command: ros2 run demo_nodes_cpp talker }
listener-d11:  { environment: { ROS_DOMAIN_ID: 11 }, command: ros2 run demo_nodes_cpp listener }
listener-d99:  { environment: { ROS_DOMAIN_ID: 99 }, command: ros2 run demo_nodes_cpp listener }
```

三個 service 都使用 `network_mode: host` + `ipc: service:talker-d11`(沿用 Phase 24 的修法)。這樣 demo 只剩一個變因:**Domain ID**。

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

在 ROS 2 Humble 裡,只要設定 `ROS_DISCOVERY_SERVER`,Fast DDS client 會自動進入 super-client 行為:它會主動接收 server 推送的 endpoints,不用再額外寫 Fast DDS XML profile。

---

## 🚀 完整 Demo 流程

### 前置:Build phase20 image(只要一次)

```bash
cd /mnt/d/ros_learn/ros2-learning-notes
docker compose -f phase-20-multi-machine/code/domain-id-demo/docker-compose.yml build
```

Demo 2 會沿用同一個 image,不用再 build 一次。

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
                                                    ← ❌ 沒有 listener 輸出,代表已隔離
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

ROSject 裡開三個 WebShell tab,分別設定:

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

這跟本機 Demo 1 的概念相同:Domain ID 一樣才會互通,不同就彼此隔離。

### Demo 2(Discovery Server)在雲端

ROSject 內通常不能使用 Docker daemon,但 discovery server 本身不一定要透過 Docker 跑。你可以直接在 WebShell 啟動 `fastdds` 工具:

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

效果跟本機 demo 類似,只是少了「多 container / 多機器」的部署感。本機 Docker 版比較能呈現 Discovery Server 在工程上的價值。

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

**原因**:DDS 會用 domain id 推算 UDP port(`base + 250 * domain_id`)。**0–101 是相對安全的範圍**;超過 101 容易撞到系統其他服務的 port,超過 232 則會超出 65535 而無法使用。

**解**:同 host 不同 ROS 系統,用 0–101 即可,例如 11、22、42。

### ⚠️ 雷 3:Discovery Server 開 bridge network 客戶端註冊不上去

**症狀**:`fastdds discovery` server 起來了,server log 看 "Server is running",但 client(talker / listener)的 `ros2 topic list` 永遠看不到對方的 topic。

**原因**:在 Docker 預設 bridge network + Humble FastRTPS 2.6.11 的組合下,client 雖然可以用 unicast 連到 server IP,但 server 轉回來的 endpoint metadata 不一定能穩定送達。這裡牽涉 Fast DDS 與 Docker/WSL 網路 stack 的細節,教學上不把它當主線展開。

**解(本章採用)**:**全部走 host network**(`network_mode: host` + `ROS_DISCOVERY_SERVER=127.0.0.1:11811`)。雖然 host network 下 multicast 原本就能通,但這裡的重點是練習未來跨真實機器部署時的設定方式:把 `127.0.0.1` 換成 discovery server 的實機 IP,整套模式就能延伸出去。

**進階**:如果一定要 bridge network,需要寫 fastdds XML profile 設定 super-client + 明確列 server 的 IP,並設 `FASTRTPS_DEFAULT_PROFILES_FILE`。範例 XML 見 [eProsima 文件](https://fast-dds.docs.eprosima.com/en/latest/fastdds/discovery/discovery_server.html)。

### ⚠️ 雷 4:`RMW_IMPLEMENTATION` 沒設成 fastrtps

**症狀**:client 設了 `ROS_DISCOVERY_SERVER=...` 但完全沒效果,client 行為跟沒設一樣(走 multicast)。

**原因**:**Discovery Server 是 Fast DDS 的功能**。如果你的 system 預設 RMW 是 Cyclone DDS,`ROS_DISCOVERY_SERVER` 會被忽略,行為看起來就像沒有設定。

**解**:強制設 `RMW_IMPLEMENTATION=rmw_fastrtps_cpp`,並確認 `ros-humble-rmw-fastrtps-cpp` 套件有裝。

### ⚠️ 雷 5:`/tmp` 在 WSL 會被 cleanup

**症狀**:用 `setsid ... > /tmp/foo.log 2>&1 &` 跑 background talker,過幾分鐘 log 檔不見了 / process 也消失了。

**原因**:WSL2 可能會清理 `/tmp`,而且 background process 在某些情況仍然跟當次 `wsl` 命令的 session 綁在一起。命令結束時,child process 也可能一起被收掉。

**解**:
1. log 寫到 `~/p20_logs/` 之類的家目錄,不是 `/tmp`
2. 教學驗證直接用 Docker container 跑,讓 process、log、網路設定都比較可預測

這也是本章用 Docker 模擬多機器的原因:它比手動開一堆 background process 穩定很多。

### ⚠️ 雷 6:Discovery Server 只設一邊

**症狀**:talker 設了 `ROS_DISCOVERY_SERVER`,listener 沒設 → 兩邊看不到彼此。

**原因**:Discovery Server 的規則是:**所有要互通的 client 都要指到同一個 server**。只有 talker 設定時,talker 會去跟 server 註冊,但 listener 還停在 multicast 模式,自然找不到 talker。

**解**:任何要加入這個 ROS 系統的 process,都要設一樣的 `ROS_DISCOVERY_SERVER`。可以放 `~/.bashrc` 或 systemd unit 環境變數,實機部署時統一設定。

---

## 🎯 學到的關鍵概念

| 概念 | 一句話 |
|------|------|
| `ROS_DOMAIN_ID` | DDS 的隔離邊界,不同 domain 的 ROS graph 彼此看不到 |
| Discovery Server | 用 server-client discovery 取代 multicast,適合 multicast 被擋的網路 |
| Super-Client | 會主動接收 server 推送 endpoints 的 client,Humble 設 `ROS_DISCOVERY_SERVER` 後會自動啟用 |
| `RMW_IMPLEMENTATION` | Discovery Server 需要 Fast DDS,Cyclone DDS 不支援這個功能 |
| Docker Compose 當「假多機」 | 比真的準備兩台電腦更可控,適合教學、debug 和 CI |

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
└── images/                                ← (可選:補 log 或架構截圖)
```
