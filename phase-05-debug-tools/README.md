# Phase 05：Debug 工具集

**學完你會**：用四個 ROS 2 內建工具看清楚整個系統在做什麼——畫出通訊圖、錄製重播訊息、即時繪圖、集中看 log。**這些是每天都會用的東西，佔 ROS 開發時間的 50%**。

**前置**：[Phase 01](../phase-01-cloud-env-first-publisher/) 已能跑（場景中要有節點在通訊才有東西可看）。

**產出**：純工具操作章節，無新 code。

---

## 為什麼 Debug 工具比新概念重要

你前面學了 Pub/Sub/Service。但實務上 ROS 系統有**幾十個節點**，光看程式碼根本搞不懂誰跟誰講話。再加上 ROS 2 的「靜默失敗」特性（QoS 不匹配、namespace 不對、topic 名稱拼錯），不開工具看根本沒辦法 debug。

本章學的順序：
1. **`rqt_graph`** —— 看通訊架構圖
2. **`ros2 bag`** —— 錄製/重播訊息
3. **`rqt_plot`** —— 即時繪圖
4. **`rqt_console`** —— 集中查看 log

---

## 🛠️ 準備：起一個有東西可看的場景

每個工具都需要「有節點在跑」才有東西可看。先把 turtlesim + auto_drive 起來。

### Terminal 1
```bash
ros2 run turtlesim turtlesim_node
```

### Terminal 2
```bash
ros2 run phase01_pkg auto_drive --ros-args -r cmd_vel:=/turtle1/cmd_vel
```

> 註：本章用 `phase01_pkg` 是因為作者把每個 phase 的 cpp 套件都放進同一個 workspace 並重新命名，方便本機練習。在你的環境用實際的套件名即可。

---

## 🛠️ 工具 1：`rqt_graph` — 看通訊架構圖

### 啟動
```bash
rqt_graph
```

預設視窗開出來可能空白，要做幾件事讓它顯示：

1. **左上下拉選單**：從 `Live` 改成 `Nodes/Topics (all)` 或 `Nodes only`
2. **點 Refresh 按鈕** 🔄（左上藍色循環箭頭）：強制重新掃描
3. **取消勾選 "Hide: Dead sinks"、"Leaf topics"、"Debug"**：學習階段全部顯示

### 你會看到的圖

```
[/auto_drive_node] ──/turtle1/cmd_vel──▶ [/turtlesim]
```

- **橢圓** = 節點 (Node)
- **方框** = topic（顯示 topic 時，目前模式只顯示節點所以箭頭上面標 topic 名稱）
- **箭頭** = 資料流方向（從 publisher 指向 subscriber）

### 實驗 1：殺掉 publisher，看圖怎麼變

在 Terminal 2 按 `Ctrl+C` 停掉 auto_drive，然後在 rqt_graph 按 Refresh 🔄。

**你會看到** `auto_drive_node` 那個橢圓**消失**，箭頭也斷了——只剩 `/turtlesim` 一個孤獨的節點。

🎯 **debug 思路**：「我的命令沒發出去？」→ 開 rqt_graph → 發現 publisher 根本不在圖上 → 節點沒啟動或 crash 了。

### 實驗 2：故意拼錯 topic 名稱

恢復 auto_drive，但故意用錯誤的 topic 名稱：

```bash
# Terminal 2 — 注意：故意打錯成 cmd_velocity
ros2 run phase01_pkg auto_drive --ros-args -r cmd_vel:=/turtle1/cmd_velocity
```

回 rqt_graph 按 Refresh，**取消勾選 "Hide: Dead sinks"**（這很重要，預設它會藏掉「沒有訂閱者的 topic」）。

**你會看到**：

```
[/auto_drive_node] ──/turtle1/cmd_velocity──▶  (孤兒 topic，沒有訂閱者)

[/turtlesim] (在訂閱 /turtle1/cmd_vel，但沒人發)
```

**箭頭斷了**——publisher 在發 `cmd_velocity`，subscriber 在聽 `cmd_vel`，兩條 topic 不相連。烏龜不會動但不會報錯（**ROS 2 的「靜默失敗」**）。

🎯 **這是 ROS 開發者每天 debug 50% 時間在抓的 bug**：topic 名稱對不上、namespace 錯、QoS 不匹配。**rqt_graph 一眼看出**——比看程式碼快 100 倍。

### 修好它

```bash
# Ctrl+C 停掉錯誤的版本，跑正確的
ros2 run phase01_pkg auto_drive --ros-args -r cmd_vel:=/turtle1/cmd_vel
```

Refresh，箭頭恢復正常。

---

## 🛠️ 工具 2：`ros2 bag` — 錄製 / 重播訊息

bag 是 ROS 最強的 debug 工具。它把指定的 topic 錄成 SQLite 檔，之後可以重播——像錄遊戲畫面，事後慢慢看。

### 實務應用
- 跑實體機器人時錄下感測器資料，回實驗室慢慢分析
- Bug 重現：錄下出 bug 那一刻，事後反覆 replay 找原因
- 訓練資料收集：錄一堆 sensor data 給機器學習用
- 整合測試：錄一段「正確輸入」當測試 fixture

### 錄製

新開一個 terminal（保留 turtlesim 與 auto_drive 在跑）：

```bash
mkdir -p ~/bags && cd ~/bags
ros2 bag record /turtle1/cmd_vel /turtle1/pose -o my_first_bag
```

按 `Ctrl+C` 停止錄製。`-o my_first_bag` 是輸出資料夾名稱。

### 看錄了什麼

```bash
ros2 bag info my_first_bag
```

輸出範例：
```
Files:             my_first_bag_0.db3
Bag size:          33.4 KiB
Duration:          2.0s
Messages:          133
Topic information:
  Topic: /turtle1/cmd_vel | Type: geometry_msgs/msg/Twist | Count: 1
  Topic: /turtle1/pose    | Type: turtlesim/msg/Pose      | Count: 132
```

可以看到：
- **檔案大小、時長、總訊息數**
- 每個 topic 的**型別、訊息數量**

> 觀察重點：`/cmd_vel` 只有 1 筆而 `/pose` 有 132 筆。這是因為 turtlesim 高頻發 pose（每秒 60 多次），而 auto_drive 已經過了 3 秒煞車期，只發了 1 筆「速度=0」就一直保持。

### 重播

先停掉 auto_drive（Ctrl+C），但 **turtlesim 留著**。然後：

```bash
ros2 bag play my_first_bag
```

烏龜會「重現」剛剛錄到的動作——因為 bag 把錄到的 `/turtle1/cmd_vel` 重新發一次給 turtlesim 訂閱。

🎯 **重點**：bag 不分原本的 publisher 是誰，**它只負責把訊息再丟到 topic 上**。turtlesim 不知道訊息是從原 auto_drive 還是 bag 來的，照樣處理。**這就是 ROS 鬆耦合架構的威力**。

### 進階用法

```bash
# 錄所有 topic（小心硬碟空間）
ros2 bag record -a

# 錄 5 秒就自動停
ros2 bag record /turtle1/cmd_vel --max-cache-size 5000000 -d 5

# 用過濾器，只錄 cmd_vel 開頭的 topic
ros2 bag record -e "/cmd.*"

# 重播時加速 2 倍
ros2 bag play my_first_bag --rate 2.0

# 重播時 loop（不斷重複）
ros2 bag play my_first_bag --loop
```

---

## 🛠️ 工具 3：`rqt_plot` — 即時繪圖

把 topic 內的數值欄位畫成即時曲線。例如看烏龜的 `pose.x` / `pose.y` 隨時間怎麼變——眼睛抓 pattern 比讀數字快得多。

### ⚠️ 雷：Humble 沒有 `rqt_plot` 獨立執行檔

如果你打 `rqt_plot` 會看到 `command not found`。Humble 把它做成 plugin，要透過 `rqt` 主程式啟動：

```bash
rqt --standalone rqt_plot.plot.Plot
```

`rqt_console` 也是同樣模式：

```bash
rqt --standalone rqt_console.console.Console
```

> ROS 2 對 rqt 子工具的封裝方式版本之間略有差異，舊版 (Foxy) 可以直接 `rqt_plot`，Humble 要走 plugin 路徑。

### 操作步驟

1. **啟動 rqt_plot**（terminal 會出新視窗）：
   ```bash
   rqt --standalone rqt_plot.plot.Plot
   ```

2. **加入要監看的 topic 欄位**：
   - 視窗上方 `Topic:` 輸入框輸入 `/turtle1/pose/x`
   - 按右邊 `+` 按鈕加入
   - 同樣加入 `/turtle1/pose/y`
   - 兩條彩色曲線出現（藍 = x、紅 = y）

3. **讓烏龜動起來看曲線變化**：
   ```bash
   # 另開 terminal，發 8 秒「邊前進邊旋轉」指令
   ros2 topic pub /turtle1/cmd_vel geometry_msgs/msg/Twist \
     '{linear: {x: 2.0}, angular: {z: 1.5}}' --rate 10
   # Ctrl+C 結束
   ```

4. 你會看到 **兩條 sin/cos 形狀的波**互相錯開 90 度——這就是圓周運動的數學本質：x = r·cos(θ), y = r·sin(θ)。

### 重點設定

- **右上角 ✓ autoscroll**：勾選 → 圖表自動跟時間捲動。不勾 → 畫面凍結，曲線會跑出畫面。
- **底下工具列的「圖例」按鈕**：可以隱藏特定曲線，多 topic 時很有用。
- **滑鼠拖曳 / 滾輪**：可以縮放與移動視窗。

### 實務應用

- **PID 調參**：訂閱 `/error` topic 畫成曲線，看誤差有沒有收斂
- **感測器融合驗證**：把 IMU 的 yaw 與 odom 的 yaw 同時畫，比對是否一致
- **效能診斷**：訂閱 `/control_loop_time` 看 callback 是否抖動

---

## 🛠️ 工具 4：`rqt_console` — 集中查看 log

ROS 系統有幾十個節點時，每個節點都在 log，混在 terminal 根本看不清。rqt_console 把所有節點的 log 集中、可過濾、可搜尋。

### 啟動

```bash
rqt --standalone rqt_console.console.Console
```

（同樣是 plugin 路徑，不是 `rqt_console` 直接執行。）

### 製造一些 log

讓 Phase 04 的 `auto_brake_service` + 障礙物模擬器跑著，會持續產生 WARN log：

```bash
# Terminal 1: fake lidar with obstacle at 0.5m
python3 /tmp/fake_lidar.py 0.5

# Terminal 2: auto_brake_service (will keep WARNing)
ros2 run phase04_pkg auto_brake_service
```

### 你會在 rqt_console 看到

```
[WARN]  auto_brake_service_node  Obstacle detected at 0.50m! BRAKING!
[WARN]  auto_brake_service_node  Obstacle detected at 0.50m! BRAKING!
[INFO]  auto_brake_service_node  AEB Service ready: 'toggle_brake'
```

訊息會即時滾動進來，最新的在最上面。

### 過濾功能（這才是真正的價值）

- **Severity 下拉**（左上）：只顯示 Debug / Info / Warn / Error / Fatal 等級
- **Node filter**（右上）：只顯示來自某個節點的 log
- **Message filter**（搜尋框）：用關鍵字過濾訊息內容
- **Highlight 模式**：不過濾，只把符合條件的訊息標亮

### 實務情境

跑 Nav2 時有 **50 個節點同時 log**，每秒幾百行訊息。用 rqt_console：
- Severity 設 ERROR → 立刻看到所有錯誤
- Node filter 設 amcl → 只看定位節點
- 結合兩者 → 「amcl 出了什麼錯」5 秒內找到

---

## 🐛 常見雷與 debug 思路

### 雷 1：rqt 視窗凍結但不報錯
殺掉 publisher 之後 rqt_plot / rqt_console 不會關閉，曲線變成水平直線、log 停止滾動。**沒有錯誤訊息**。看到「圖凍結」要回頭看節點還活著嗎：`ros2 node list`。

### 雷 2：rqt_graph 預設藏太多
`Hide: Dead sinks` 預設勾選，會藏掉「沒人訂閱的 topic」——但這常常正是 bug 的線索。學習階段把所有 Hide 都取消勾。

### 雷 3：ros2 daemon 偶爾 stuck
SIGKILL 太用力殺節點時，`ros2 node list` 可能會 hang 或丟 `!rclpy.ok()`。解法：
```bash
ros2 daemon stop
ros2 daemon start
```

### Debug 標準流程
1. 開 `rqt_graph` 看通訊圖 → 確認節點存在、箭頭連通
2. 用 `ros2 topic echo <topic>` 看訊息有沒有真的在流
3. 用 `ros2 topic hz <topic>` 看頻率是否符合預期
4. 開 `rqt_console` 過濾 ERROR/WARN
5. 還抓不到 → `ros2 bag record` 一段，回頭慢慢分析

---

## 📋 Phase 05 工具速查表

| 工具 | 啟動指令 | 用途 | 看什麼 |
|------|---------|------|-------|
| `rqt_graph` | `rqt_graph` | 通訊架構 | 節點與 topic 連線 |
| `ros2 bag record` | `ros2 bag record -a` | 錄製訊息 | SQLite 檔 |
| `ros2 bag info` | `ros2 bag info <bag>` | 看錄了什麼 | topic、訊息數、時長 |
| `ros2 bag play` | `ros2 bag play <bag>` | 重播訊息 | 重現當時的 topic 流 |
| `rqt_plot` | `rqt --standalone rqt_plot.plot.Plot` | 即時繪圖 | 數值欄位的時間曲線 |
| `rqt_console` | `rqt --standalone rqt_console.console.Console` | 集中 log | 所有節點訊息 + 過濾 |

---

## 🎯 學到的關鍵概念

- **rqt_graph 是 debug 第一招**：topic 沒收到 → 開圖看箭頭斷在哪
- **ROS 2 靜默失敗**：topic 名稱錯、QoS 不匹配都不會報錯，只能用工具看
- **`Hide: Dead sinks` 很雞肋**：學習階段一律取消勾選
- **bag 是 ROS 殺手級功能**：實體機器人錄一次，實驗室分析一輩子
- **bag 與發送者解耦**：subscriber 不知道訊息來自原 publisher 還是 replay

---

## 下一步

學會看系統了，接下來學「讓系統可調整」：
- Phase 06 — Parameters（讓常數可從外部動態調整，不用重編譯）
