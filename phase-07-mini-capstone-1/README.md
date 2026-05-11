# Phase 07：Mini Capstone 1 — 智能煞車車小專題 🏎️

**學完你會**：把我們在 Phase 03、04、06 學過的各路武功**完美融合**，打造出一個真的可以拿來炫耀的小專題——一台能遠端遙控開關、隨時動態調速、遇到障礙物還會聰明減速的烏龜車！這包做完，花個下午丟到 GitHub 上，你的作品集立刻多一筆亮點。

**前置準備**：
- [Phase 03](../phase-03-subscriber-lidar-brake/) — 訂閱光達資料與 QoS
- [Phase 04](../phase-04-services-toggle/) — 架設 Service Server
- [Phase 06](../phase-06-parameters/) — Parameters 與動態變更攔截器

**產出**：
- [`code/my_cpp_pkg/src/smart_brake.cpp`](code/my_cpp_pkg/src/smart_brake.cpp) — 我們的主角：超強整合節點
- [`code/my_cpp_pkg/launch/capstone.launch.py`](code/my_cpp_pkg/launch/capstone.launch.py) — 只要一行指令就能叫醒整個世界的 Launch 檔（**偷偷預習 Phase 09**）

---

## 🤔 為什麼我們需要這個 Capstone？

老實說，前面學到 Phase 06，你手邊已經握有 6 大 ROS 2 神器（Pub / Sub / Service / QoS / Param / Debug 工具），但這些就像是一盤散沙的「**單元測試**」——上完課好像懂了，但要你自己組一台車，腦袋可能就當機了。

這個 Mini Capstone 1 就是要逼著你把這些東西**通通揉進同一個 Node 裡面**，並且設計出一個**自帶戲劇效果的 Demo**：一邊拖滑桿改速度、一邊按 Service 開關，然後看著螢幕上的烏龜隨著你的指令翩翩起舞。親手整合過一次，之後 Phase 08 開始打大魔王（進階主題）時，你的底盤才會夠穩！

---

## 🏗️ 系統大解密

看圖說故事：
```
        Parameters (用 rqt_reconfigure 即時調)
          ├── max_speed
          ├── safe_distance
          └── corridor_width
                   │
                   ▼
[fake_lidar] ──/lidar_points──▶ [smart_brake_node] ──/cmd_vel──▶ [turtlesim]
                                       ▲
                                       │
[ros2 service call] ──/toggle_brake────┘
```

我們的 `smart_brake_node` 在這裡可是個大忙人，一人分飾 **5 角**：
1. **Publisher（發佈者）**：負責發送 `cmd_vel` 來奴役烏龜。
2. **Subscriber（訂閱者）**：負責聽 `PointCloud2` 的光達資料，計算哪裡會撞到。
3. **Service Server（服務端）**：提供 `toggle_brake` 開關，隨時切換避障模式。
4. **Parameter Holder（參數總管）**：保管 3 個 Param，並且守在攔截器抓壞資料。
5. **Timer Owner（計時大師）**：每 100 毫秒穩定發送一次 `cmd_vel`（這很重要，我們不靠光達來觸發動作，這樣就算光達當機，車子還是有控制權）。

---

## 🚀 跟 Phase 04 的傻瓜車比，哪裡不一樣？

| 比較項目 | Phase 04 的 `auto_brake_service` | Phase 07 的 `smart_brake` (本作主角) |
|------|-----------------------------|---------------------|
| 看到障礙物時 | 嚇傻直接停在原地 (速度=0) | **優雅減速到 `max_speed` × 0.3** (保持緩速移動) |
| 正常速度 | Hardcode 寫死 0.2 | 聽命於 Param `max_speed` (隨你調) |
| 安全距離 | Hardcode 寫死 1.0 | 聽命於 Param `safe_distance` |
| 啟動方式 | 手敲好幾個 `ros2 run` 終端機 | **靠 Launch file 一鍵喚醒所有 Node + Remap** |
| Demo 觀賞性 | 只能盯著枯燥的 log | **螢幕上的烏龜真的會跑、會停、會煞車！** |

💡 **為什麼要改成「障礙物近 = 減速不停」？** 
因為這樣才能讓烏龜**保持動作**！當烏龜一直在跑的時候，你去拖動 `max_speed` 滑桿，才能用肉眼直觀感受到「喔！同樣有障礙物，速度真的跟著改變了耶！」。

---

## 💻 程式碼核心亮點

完整程式碼在 [`code/my_cpp_pkg/src/smart_brake.cpp`](code/my_cpp_pkg/src/smart_brake.cpp)。來看看幾個高光時刻：

### 1. 用 `std::atomic` 搞定多執行緒大亂鬥

```cpp
std::atomic<double> max_speed_;
std::atomic<double> safe_distance_;
std::atomic<bool> brake_enabled_{true};
std::atomic<double> last_min_distance_{100.0};
```

因為我們的 Node 裡有四種 Callback (Service、Param、Lidar、Timer)，它們就像四個不講武德的工人，隨時會搶著讀寫變數。用 `atomic` 就像給變數上了鎖，保證不會發生 Race Condition 悲劇。

> ⚠️ **踩坑警告**：如果你直接寫 `RCLCPP_INFO("...%.2f", max_speed_)`，編譯器會賞你 50 行紅字（因為 `atomic<double>` 的 copy constructor 被拔掉了）。**記得要乖乖用 `.load()` 來拿值喔！** 我自己第一次寫就被坑過啦。

### 2. 聰明的三段變速邏輯

```cpp
if (!brake_enabled_) {
    twist.linear.x = max_speed;                  // 防護罩關閉 → 閉著眼睛全速衝！
} else if (dist > safe_distance) {
    twist.linear.x = max_speed;                  // 前方暢通 → 全速前進！
} else {
    twist.linear.x = max_speed * 0.3;            // 偵測到障礙物 → 縮到 30% 速度慢慢嚕
}
```

這三層 `if-else` 就是你等一下 Demo 時會看到的三種行為模式。

### 3. Launch File 搶先體驗

看看 [`launch/capstone.launch.py`](code/my_cpp_pkg/launch/capstone.launch.py)，它能一次幫你開好 Node、接好管線（Remap）、還順便塞好預設參數：

```python
Node(
    package='phase07_pkg',
    executable='smart_brake',
    name='smart_brake_node',
    output='screen',
    remappings=[('cmd_vel', '/turtle1/cmd_vel')],
    parameters=[{
        'max_speed': 0.5,
        'safe_distance': 1.0,
        'corridor_width': 0.4,
    }],
)
```

Phase 09 我們會認真聊 Launch File，現在你先體驗一下「一行指令喚醒千軍萬馬」的快感就好！

---

## 🎬 Showtime：Demo 劇本教戰守則

### Step 1：建置 + 一鍵啟動

```bash
cd ~/ros2_ws
colcon build --packages-select phase07_pkg
source install/setup.bash
ros2 launch phase07_pkg capstone.launch.py
```

這時 Turtlesim 視窗會彈出來，烏龜開始**以 0.5 m/s 的時速歡樂向前跑**（因為前面沒東西）。

![turtlesim 視窗，烏龜留下從中央到右邊牆的長軌跡](images/turtle_full_speed_trail.png)

> 注意看白色的軌跡線！軌跡長度 = `max_speed` × 時間。因為我們現在設定 0.5，比 Phase 01 的 0.2 快多了，所以軌跡明顯變長。

### Step 2：開啟 GUI，滑桿嚕起來

開個新終端機來玩 GUI：

```bash
rqt --standalone rqt_reconfigure.param_plugin.ParamPlugin
```

找到 `/smart_brake_node`，試著把 `max_speed` 往上調。

![rqt_reconfigure 顯示 max_speed=2.0 加 turtlesim 同步顯示烏龜超快軌跡](images/rqt_param_max_speed_2.png)

> 當你把 `max_speed` 改成 2.0 時，烏龜會像是嗑了藥一樣**以 4 倍速往右牆衝刺**！
> 
> 如果你調皮試試看 6.0，就會被我們寫的門神擋下來：
> `Setting parameter failed: max_speed must be in [0, 5.0]`

### Step 3：召喚障礙物 + Service 切換大法

再開一個終端機：

```bash
# 在烏龜正前方 0.5m 處變出一個隱形牆
python3 ~/fake_lidar.py 0.5
```

Node 收到光達訊號後，馬上切換成「減速到 30%」模式，你會看到烏龜瞬間軟腳變慢。

這時，我們再開一個終端機，發送 Service 把防護罩關掉：

```bash
ros2 service call /toggle_brake std_srvs/srv/SetBool "{data: false}"
```

![smart_brake terminal log 顯示 Slowing → DISABLED → FULL SPEED 三段切換](images/smart_brake_log.png)

> **這張 Log 截圖充滿了故事**：
> - 上面黃色的字：`[WARN] Obstacle 0.50m. Slowing...` ← 看到障礙物，啟動減速。
> - 中間那行：`[WARN] >>> Service: brake DISABLED <<<` ← 你剛才呼叫 Service 關掉防護。
> - 下面白色字：`[INFO] Brake offline. FULL SPEED...` ← 系統無視障礙物，油門踩到底。
> 
> 仔細看時間戳記（Timestamp），從你呼叫 Service 到系統狀態更新，**中間只差了 37 毫秒**！這就是 Service 的威力：**隨叫隨到，下一秒就反映在動作上**。

這時候，你會看到烏龜**瞬間從 0.15 m/s 暴衝到 0.5 m/s** 撞向牆壁。

### Step 4：把防護罩重新打開

```bash
ros2 service call /toggle_brake std_srvs/srv/SetBool "{data: true}"
```

烏龜又會立刻乖乖減速——因為那面 0.5m 的隱形牆還在那裡，攔截器再次介入。

---

## 🎯 經驗值結算

來盤點一下你這章用了哪些招：

| 來源 | 你的應用 |
|------|------------------|
| Phase 01 | Publisher 三大要素、靠 Timer 穩定發送訊號 |
| Phase 03 | Subscriber 接收資料、SensorDataQoS、解析 PointCloud2 |
| Phase 04 | 開發 SetBool Service、用 `atomic` 保護共享狀態 |
| Phase 05 | （雖然沒用 rqt_graph，但強烈建議你自己開來看連線圖） |
| Phase 06 | `declare_parameter` 宣告、`on_set_callback` 攔截驗證 |

**這章解鎖的新成就**：
- 學會**多個 Callback 共用狀態**的設計模式。
- 掌握 **Timer-driven** 的控制法（不被 Sensor 的發送頻率綁架）。
- 初嘗 **Launch File** 的甜頭，用 Python 腳本搞定啟動流程。

---

## 🌟 大師級挑戰區

如果你覺得不過癮，可以試試看這幾個挑戰：

1. **加入方向盤（Angular Control）**：如果障礙物在右邊，烏龜就向左轉；在左邊就向右轉。（提示：需要把 corridor 邏輯拆開算左右兩邊的距離）。
2. **減速比例做成參數**：把程式裡寫死的 `0.3` 抽出來，做成一個叫做 `obstacle_slowdown_factor` 的 Param。
3. **滑順的線性減速**：不要用「低於距離就瞬間砍到 30%」，改成數學線性內插：「距離=安全 → 速度 100%；距離=0 → 速度 0%」，讓煞車變絲滑。
4. **里程計算法**：訂閱 `/turtle1/pose` 來算烏龜總共跑了多遠，把移動距離當作動態加減速的依據。

只要你挑一個做完，**馬上把 GitHub 連結丟到履歷上吧！** 因為這個專題已經能強力證明：「我懂 ROS 的全套通訊機制，而且我真的知道怎麼把它們整合在一起！」

---

## 👣 下一步去哪？

恭喜你順利通過第一關的整合考驗！接下來，我們要進入 **Part 3：系統設計篇**。
從現在開始，我們不再只拿別人寫好的工具來用，我們要開始**設計屬於自己的 ROS 系統**了！

- [Phase 08 — Custom Interfaces](../phase-08-custom-interfaces/)：教你如何自定義專屬的 .msg / .srv / .action，設計你自己的通訊協定！

---

## 📁 檔案結構懶人包

```
phase-07-mini-capstone-1/
├── README.md                     ← 也就是這篇攻略文
├── code/
│   └── my_cpp_pkg/
│       ├── package.xml
│       ├── CMakeLists.txt
│       ├── src/
│       │   └── smart_brake.cpp   ← 我們的主角節點
│       └── launch/
│           └── capstone.launch.py ← 一鍵啟動的魔法陣
└── images/
    ├── turtle_full_speed_trail.png
    ├── rqt_param_max_speed_2.png
    └── smart_brake_log.png
```
