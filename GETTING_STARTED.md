# 新手快速入門

> 給「**會 C++、聽過機器人、但從沒碰過 ROS**」的你。讀完這份你會知道:
> 1. ROS 2 是什麼(2 分鐘版本)
> 2. 第一次該從哪裡開始(別讀完整本 README)
> 3. 學完前 7 章你就能寫出第一個小作品

預計時間:**讀完這頁 10 分鐘** + **跑完入門 5 章 8 小時**(可分 2–3 個下午)

---

## 🤔 ROS 2 是什麼?(寫過 C++/IoT 的人版本)

**一句話**:ROS 2 = **機器人軟體用的「訊息匯流排」+ 「套件管理工具」**。

如果你寫過 IoT 系統,大概做過這種事:**一個程式抓感測器、一個程式控制致動器、它們之間用 MQTT 串訊息**。ROS 2 就是把這套工程模式做到極致 — 內建強型別訊息、自動服務發現、QoS 控制、長任務管理。

### 用你熟的東西對照

| 你熟悉的 | ROS 2 的對應 |
|---------|-------------|
| MQTT broker | **DDS**(去中心化,不需要 broker) |
| MQTT topic | **Topic**(但**強型別** — `geometry_msgs/Twist`,不是隨便丟 bytes) |
| HTTP/gRPC API | **Service**(一問一答) + **Action**(長任務 + 進度回報 + 可取消) |
| docker-compose | **launch file**(一次起多個 node) |
| systemd service | **LifecycleNode**(unconfigured → inactive → active 五狀態) |
| `pip install` | **`apt install ros-humble-XXX`** + **colcon build** 編譯自己的套件 |

> **想看完整對比?** → [Phase 02:ROS 2 設計哲學](phase-02-communication-concepts/) 有完整的 ROS 2 vs MQTT vs gRPC vs ROS 1 對照。但**先別讀**,跑完 Phase 01 再回去看才有感覺。

### ROS 2 系統長什麼樣?(觀念地圖)

把機器人想成「**很多獨立 C++ 程式各自做一件事,透過 ROS 2 互通**」:

```
        ┌─────────────────────────────────────────────────┐
        │              DDS 共享匯流排(無 broker)           │
        │   所有 Node 都連到這條,自動發現彼此,點對點通訊    │
        └─────────────────────────────────────────────────┘
            ▲          ▲          ▲          ▲
            │          │          │          │
       ┌────┴────┐ ┌──┴────┐ ┌───┴───┐ ┌────┴────┐
       │ 光達讀取 │ │ 避障  │ │ 馬達  │ │ 鍵盤   │
       │  Node   │ │ Node  │ │ 控制  │ │ 遙控   │
       └─────────┘ └───────┘ └───────┘ └─────────┘
```

**通訊有 3 種機制**(這是你會用一輩子的東西):

| 機制 | 比喻 | 範例 | 對應你熟的 |
|------|------|------|-----------|
| **Topic** | 廣播電台 | 「我每秒發一筆光達資料」 | MQTT pub/sub |
| **Service** | 打電話 | 「請打開避障」→「好,已開」 | HTTP REST / gRPC |
| **Action** | 叫外送 | 「導航到 (3,5)」→「30% 進度」→「完成 / 我中途取消」 | gRPC streaming |

**還有「形體」與「治理」層**(Part 3+ 會學):

```
              你的機器人專案
                    │
  ┌─────────────────┼─────────────────┐
  │                 │                 │
通訊層            形體層           治理層
Topic/Service    URDF (描述身體)   Launch (一次起多個 Node)
Action           TF2  (座標轉換)   Lifecycle (五狀態管理)
QoS              Gazebo (模擬)     Parameters (動態調參)
                                    Testing (gtest)
```

**你不需要一次學完**。從通訊層的 Pub/Sub 開始,自然就會知道下一個該學什麼。

### 為什麼不直接用 MQTT 寫機器人?

幾個會踩死你的點:
1. **MQTT 沒型別** — 你發 `linear.x = 0.2`,要自己 JSON 編碼/解碼,各端都可能寫錯
2. **MQTT 沒 RPC** — 「打開避障」這種一問一答的呼叫,要自己用兩個 topic 模擬 request/response
3. **MQTT 沒長任務** — 「導航到 (3, 5),預計 30 秒」這種要回報進度、可取消的任務,沒原生支援
4. **MQTT 必須有 broker** — broker 掛了全部 GG;ROS 2 的 DDS 沒這個單點

ROS 2 把這些痛點全包了。代價是學習曲線比較陡。

---

## 🚀 第一次該從哪開始?

### Step 1:選一個環境(5 分鐘)

兩種選擇,**初學者強烈建議從 ☁️ 雲端開始 — 完全免裝**:

| 環境 | 適合 | 設定時間 |
|------|------|---------|
| ☁️ **TheConstructSim 雲端** | 想 5 分鐘內看到車子動,**完全不想裝任何東西** | 0 分鐘(註冊即用) |
| 💻 **本機 WSL2 + Ubuntu** | 已有 WSL 經驗、想長期開發 | 30–60 分鐘 |

#### ☁️ 用 TheConstructSim 免費版能走多遠?

**答案:超過 80% 的章節都能完整跑**,而且**前 7 章入門路徑全程不用裝任何東西**。

| 階段 | 雲端能不能跑 |
|------|-----------|
| **入門 7 章(Phase 01–07)** | ✅ 全部能跑,完全不需要本機 |
| **Part 3 系統設計(Phase 08–14)** | ✅ 全部能跑 |
| **Part 4 機器人形體(Phase 15–19)** | ✅ 全部能跑(Gazebo 在雲端比 WSL 順) |
| **Part 5 Track A SLAM/Nav2** | ✅ **雲端有 GPU,實際建出地圖、跑出導航**(WSL 沒 GPU 跑不出) |
| **Part 5 Track B MoveIt** | ☁️ 大部分可,Phase 21B Setup Assistant GUI 例外(需本機) |
| **Part 6 Docker/CI** | 🚫 必須本機(雲端沒 Docker daemon) |

完整章節對照表見 [SETUP.md 的「各章雲端可用性對照表」](SETUP.md#-各章雲端可用性對照表)。

#### TheConstructSim 註冊與快速上手

1. 到 [app.theconstructsim.com](https://app.theconstructsim.com/) 註冊免費帳號
2. **ROSjects** → **Create New ROSject** → 選 **ROS 2 Humble**
3. 點 `</> Open` 進入雲端虛擬機(內含 Terminal、Code Editor、Gazebo viewer)
4. 第一次先試 `ros2 topic list` 確認環境 OK
5. 想要直接拿到本 repo 全部 code:
   ```bash
   cd ~/ros2_ws/src
   git clone https://github.com/gino07172002/ros2-learning-notes.git
   ```

**免費版限制**:
- 每次連線約 1 hr,工作要中間存檔(ROSject 會保留 code,但跑中的 process 會掉)
- 不能 `sudo apt install` 自訂套件(預裝 ROS 2 + Gazebo + Nav2 + slam_toolbox 通常已夠用)
- 跑 SLAM/Nav2 這種重型 demo 可能會比本機慢,但**比 WSL 沒 GPU 強很多**

完整比較表 + 本機 WSL 安裝步驟在 [`SETUP.md`](SETUP.md)。**糾結哪個的話用雲端,跑通 Phase 01–04 後再決定要不要切本機**。

### Step 2:跟著 Phase 01 跑出第一個 Publisher(2 小時)

→ [`phase-01-cloud-env-first-publisher/`](phase-01-cloud-env-first-publisher/)

這章會帶你:
- 建一個 ROS 2 套件(等同於建一個專案資料夾)
- 寫一支 C++ 程式(`auto_drive.cpp`)讓模擬器中的車子前進 3 秒後停下
- 用 `ros2 topic echo` 看通訊背後發生什麼

**跑完這章你會理解的事**:
- ROS 2 套件結構長什麼樣(`package.xml` + `CMakeLists.txt` + `src/`)
- `colcon build` 是什麼(進階版 make)
- Publisher 怎麼寫、Topic 是什麼

### Step 3:照下面這條路徑往下走

新手只需要走這 6 章 + 一個小作品,**不用一次讀完整個 ROADMAP**:

```
Phase 01  Publisher(發訊息)         ← 你已經會了
   ↓
Phase 02  ROS 2 設計哲學(觀念深化)
   ↓
Phase 03  Subscriber(收訊息)+ QoS
   ↓
Phase 04  Service(一問一答)
   ↓
Phase 05  Debug 工具(rqt_graph 等)  ← 偵錯必備
   ↓
Phase 06  Parameters(動態調參)
   ↓
🎯 Phase 07  Mini Capstone:智能煞車車
            把上面 6 章整合成一個小作品
```

走完這 7 章 = **你會用 ROS 2 寫多節點通訊系統**。大概一個禮拜的下午時段。

---

## 🗺️ 走完入門 7 章後該做什麼?

恭喜,你現在跟 gino 開始學 ROS 之前的程度差不多 → 進階決策:

| 你想做什麼 | 該走哪條路 |
|-----------|----------|
| **架構自己的 ROS 系統**(自訂訊息、launch、寫測試) | Part 3:Phase 08–14 |
| **了解機器人的「身體」**(URDF、TF、Gazebo) | Part 4:Phase 15–20 |
| **做移動機器人**(SLAM、Nav2 自動導航) | Part 5 Track A |
| **做機械手臂**(MoveIt、抓取) | Part 5 Track B |
| **把作品上線**(Docker、CI、部署) | Part 6 |

完整地圖在 [`ROADMAP.md`](ROADMAP.md)。**每章可單獨閱讀,不用按順序全看**。

---

## ❓ 常見新手問題

### Q1:我需要先學 Linux 嗎?

要會基礎:
- 切目錄(`cd`)、看檔案(`ls`、`cat`)、編輯(`vim` / VS Code)
- 環境變數、`source` 一個 shell 檔在做什麼
- `apt install` 怎麼用

**不需要**會寫 bash script 或懂 systemd。

### Q2:我需要先學 CMake 嗎?

ROS 2 用 `colcon` 包裝過 CMake,**前 7 章你只會碰到 5 行 CMake**(都會貼給你)。
等寫到複雜套件(Phase 08+)再去理解 CMake 細節也來得及。

### Q3:Python 版在哪?

有 Python 對照版的章節,末尾會有類似 `<sub>🐍 想用 Python 寫同一個 X?看 python/。</sub>` 的引導。
但**主章用 C++ 寫,因為**:
- ROS 2 業界主力是 C++(Nav2、MoveIt、ros2_control 都是 C++)
- 學 C++ 版會逼你理解編譯、CMake、共享指標等基本功
- 只看 Python 版會錯過 50% 的精華

如果你只想用 Python,可以只看 `python/` 子資料夾,但 C++ 主章會給你更多 context。

### Q4:我會 ROS 1,直接跳哪一章?

**還是從 Phase 01 開始**。雖然觀念你大多會,但 Phase 02 會講清楚 ROS 2 為什麼跟你熟的 ROS 1 完全不同(沒 master、有 QoS、用 DDS)。
跑通後你可以直接跳 Phase 09(Executor / Lifecycle / Composition) — 那是 ROS 2 才有的東西。

### Q5:我電腦沒 GPU 會卡嗎?

前 14 章不需要 GPU(turtlesim 跟基本 Gazebo 都沒問題)。
真正吃 GPU 的是 Part 5(SLAM、Nav2、MoveIt) — WSL2 沒 GPU 會跑不順,屆時改用 ☁️ TheConstructSim 雲端版。

### Q6:每章那麼多「踩雷紀錄」是什麼?

這是這份筆記的**核心價值**:每個雷都是 gino 實際踩到、debug 修好的工程細節。
新手第一次讀**可以先跳過雷區段落**,跑通 demo 就好。等你**真的踩到那個錯誤訊息**再回頭讀,會省你幾小時。

---

## 🆘 卡住了怎麼辦?

1. **看該章的「常見雷」段落** — 9 成新手會踩的雷都在那裡
2. **跑 `ros2 topic list` / `ros2 node list` / `rqt_graph`** — 80% 的問題都是「節點沒起來」或「topic 名字不對」
3. **重編譯 + 重 source** — `colcon build && source install/setup.bash` 是 ROS 2 的「重開機」
4. **去 [GitHub issue](https://github.com/gino07172002/ros2-learning-notes/issues) 開問題**

---

## ⏭️ 準備好了嗎?

**現在就開始** → [Phase 01:雲端環境 + 第一支 Publisher](phase-01-cloud-env-first-publisher/)

---

> 📚 **想了解這個 repo 的全貌**(已完成 32+ 章,Capstone 都驗證過):看 [`README.md`](README.md) 或 [`PORTFOLIO.md`](PORTFOLIO.md)。
> 🗺️ **完整學習地圖**(Track A/B 分流):看 [`ROADMAP.md`](ROADMAP.md)。
> 🏛️ **想學「ROS 2 為什麼這樣設計」、把它當 library 設計教材讀**:跑完 Phase 02 後看 [`DESIGN_NOTES.md`](DESIGN_NOTES.md)。
