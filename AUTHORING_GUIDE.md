# 寫作指引（給未來協作的 AI / 維護者）

這份文件給未來協助維護本筆記的 AI 或人類看。**寫新章節前先看完這份**，確保風格與既有章節一致。

---

## 🎯 這個 repo 的核心設計原則

1. **每章獨立可跳讀**：讀者打開任何 phase 都能照做，不依賴前章 code。
2. **C++ 為主，Python 為輔**：主 README 用 C++ 教學，Python 版放 `python/` 子資料夾，互不干擾。
3. **TheConstructSim 與本機 WSL2 並列**：執行步驟一定要兩種環境都寫。
4. **程式碼用相對 topic 名稱**：永遠用 `cmd_vel`，由 `--ros-args -r` 對應實際環境。**絕不寫死 `/originbot_1/cmd_vel`**。
5. **筆記不只是 tutorial 翻譯**：每章要有「踩雷紀錄」「為什麼這樣寫」「常見坑」，這是這份筆記的價值所在。

---

## 📁 標準資料夾結構

新章節必須遵守這個結構：

```
phase-XX-kebab-case-topic/
├── README.md                       ← C++ 主章（觀念 + 步驟 + 程式碼）
├── code/
│   └── my_cpp_pkg/
│       ├── package.xml
│       ├── CMakeLists.txt
│       └── src/<executable_name>.cpp
└── python/                         ← 只在「值得」的章節加（見下方判斷標準）
    ├── README.md
    └── my_py_pkg/
        ├── package.xml
        ├── setup.py
        ├── setup.cfg
        ├── resource/my_py_pkg      ← 空檔案，必須存在
        └── my_py_pkg/
            ├── __init__.py         ← 空檔案
            └── <executable_name>.py
```

### 命名規則
- 資料夾：`phase-XX-kebab-case`，XX 是兩位數字（`phase-01`、`phase-15`）
- C++ 套件統一叫 `my_cpp_pkg`（每章獨立，刻意重名）
- Python 套件統一叫 `my_py_pkg`
- 執行檔用 snake_case，貼近功能（`auto_drive`、`auto_brake_service`）
- 截圖：放在 `images/` 子資料夾，**檔名一律英文 kebab-case**（不用中文檔名，git/markdown 渲染會出問題）

### 截圖規則

實際跑過的章節**強烈建議加截圖**，新讀者一眼知道「應該看到什麼樣子」。流程：

1. 章節寫完 + code 跑通的當下，順手截圖 GUI 視窗
2. 存到 `phase-XX/images/<descriptive-name>.png`
3. 在 README 對應位置插入：
   ```markdown
   ![alt text 描述圖片內容](images/xxx.png)
   ```
4. 圖下方加一段「**截圖解讀**」引言（用 `>` blockquote），點出讀者該注意什麼細節（時間戳、特殊狀態、軸標籤等）
5. 不要單純放圖不解釋——圖只是輔助，文字才是主軸

**檔名範例**：
- ✅ `rqt_graph_normal.png`、`rqt_graph_disconnected.png`、`turtlesim_with_turtle.png`
- ❌ `螢幕擷取畫面 2026-05-02.png`（中文 + 空格）
- ❌ `image1.png`、`screenshot.png`（沒語意）

---

## 🐍 該不該加 Python 對照版？

**判斷標準**：

| 該章主題 | 加 Python 嗎 |
|---------|-------------|
| 純觀念（無 code） | ❌ 不加 |
| Pub/Sub/Service/Action 等核心 API | ✅ 加（API 對照價值高） |
| QoS、Executors、Lifecycle | ✅ 加（rclpy 寫法差異大）|
| Launch File | ✅ 但反過來——Python 為主，因為 launch file **本來就是 Python** |
| Custom Interfaces (.msg/.srv) | ✅ 加（兩種語言都常用） |
| URDF / TF2 | ⚠️ 視情況——URDF 與語言無關，TF2 有 API 差異 |
| ros2_control / Nav2 / MoveIt | ❌ 通常不需要——這些主要用 C++（plugin 架構） |

**簡單規則**：如果該章是「教 ROS 2 概念與 API」就加 Python；如果是「教某個特定框架（Nav2/MoveIt）」就只用該框架慣用語言。

---

## 📝 主 README（C++ 版）寫作模板

**結構順序固定**：

```markdown
# Phase XX：<主題>

**學完你會**：<具體可驗證的能力，1–2 句>

**前置**：<明確列出依賴的章節或知識>

**產出**：[`code/my_cpp_pkg/`](code/my_cpp_pkg/) — <一句話描述>

---

## 📍 課前觀念建立 / 核心觀念

<該章必須先理解的觀念，用比喻、表格、對照圖。**不要直接跳 code**。>

---

## 🕵️ 步驟 1：終端機偵探課（適用時）

<幾乎每章都該有：用 ros2 topic / service / interface 等 CLI 工具先排查環境。
這是這份筆記的標誌性段落，省略它會少 30% 價值。>

---

## ⚠️ 關鍵知識：<該章的核心坑>

<該章最容易踩的坑——QoS、生命週期、TF 時間戳等等。
直接點出來，不要藏在步驟裡。>

---

## 💻 步驟 2：撰寫節點

<完整 .cpp 程式碼，附 inline 註解。>

> **與原筆記/官方範例的差異**：<如果你修了什麼、改了什麼，明說。>

---

## 步驟 3：CMakeLists.txt 與 package.xml

<只列「新增」的部分，完整檔案連結到 code/ 子資料夾。>

---

## 步驟 4：編譯與執行

> 兩種環境的差異只在「remap 到哪個 topic」。完整環境比較見 [SETUP.md](../SETUP.md)。

### ☁️ TheConstructSim（<具體場景>）
<bash 區塊>

### 💻 本機 WSL2（<具體場景，如 turtlesim / turtlebot3>）
<bash 區塊>

**成功指標**：<具體可觀察的結果>

---

## 🎯 學到的關鍵概念

<bullet list，3–5 條，給讀者複習用>

---

## 🌟 挑戰（選用）

<延伸練習，1 題就好>

---

## 下一步

- <連結到下一章>

---

<sub>🐍 想用 Python (rclpy) 寫同一個 <主題>？看 [python/](python/)。</sub>
```

### 強制元素檢查清單
- [ ] 標題含 Phase 編號
- [ ] 「學完你會」用具體動詞，不用「了解」「熟悉」這種空詞
- [ ] 「前置」明確列出依賴章節
- [ ] **必有** TheConstructSim + 本機 WSL2 兩種執行步驟
- [ ] **必有** Topic 用相對名稱，執行時 remap
- [ ] **必有**「成功指標」段落
- [ ] 末尾有 `<sub>🐍 ...</sub>` 引導（如果有 Python 版）

---

## 🐍 Python 子 README 寫作模板

**核心原則**：寫給 Python 開發者看，**不要假設讀者看過 C++ 版**。內容要自成一體。

C++ 讀者的提示用 `<details>` 折疊，視覺上不影響 Python 讀者：

```markdown
<details>
<summary>💡 從 C++ 過來的讀者點這裡</summary>

<API 對照表或差異說明>
</details>
```

### Python 子 README 結構

```markdown
# Phase XX（Python 版）：<主題> with rclpy

**學完你會**：<同主章但偏 Python 表述>
**前置**：<同主章>
**產出**：[`my_py_pkg/`](my_py_pkg/) — <說明>

---

## 📍 課前觀念建立
<跟主章類似，但用 Python 比喻>

## 🛠️ 步驟 1：架設環境
<TheConstruct + 本機>

## 🕵️ 步驟 2：終端機偵探課
<跟主章一樣>

## ⚠️ 關鍵知識：<同主章關鍵坑，但用 Python 寫法>

<details>
<summary>💡 從 C++ 過來的讀者點這裡</summary>
<API 對照表>
</details>

## 💻 步驟 3：撰寫節點

<完整 .py 程式碼>

<details>
<summary>💡 從 C++ 過來的讀者點這裡</summary>
<逐項對照 rclcpp ↔ rclpy 寫法>
</details>

## ⚡ NumPy 加速版（選讀，適用時）
<Python 特有：對大量資料的向量化加速>

## 步驟 4：setup.py 與 package.xml
<新增的 entry_points 與 depend>

## 步驟 5：編譯與執行
<同主章雙環境>

## 🐍 Python 開發特有提示

<這段是 Python 版的真正價值，必須有。例如：>
1. `--symlink-install` 為什麼是 Python 開發者的好朋友
2. throttle log 的兩種寫法（新舊版本相容）
3. Logger 寫法（不要用 print）
4. KeyboardInterrupt 處理
5. type hints 建議

## 🎯 學到的關鍵概念
## 下一步
- 主章 (C++)：[../README.md](../README.md)
```

### Python 版必須提供的「不只是翻譯」內容

不要把 Python README 寫成 C++ 版的逐字翻譯。**每章 Python 版都要有 C++ 版沒有的內容**：

| 範圍 | 必含主題 |
|------|---------|
| 任何章 | `--symlink-install` 開發模式 |
| 任何 callback 章 | Logger 寫法 + 不要用 `print()` |
| 處理大量資料的章（PointCloud2、image） | NumPy 向量化加速 |
| 任何章 | KeyboardInterrupt 處理（try/except 包 spin）|
| 適用章 | `throttle_duration_sec` 寫法（rclpy 較新版本特性）|

---

## 🛠️ 程式碼風格規範

### C++ (rclcpp)
- 用相對 topic 名稱（`"cmd_vel"`，不寫死前綴）
- Logger 用 `RCLCPP_INFO_THROTTLE` 避免 spam
- 多 callback 共享狀態用 `std::atomic` 或 mutex
- Subscriber 對感測器資料**必須** `rclcpp::SensorDataQoS()`
- 不要在 callback 裡阻塞（IO、長計算）
- `using std::placeholders::_1;` 而非每次都寫 namespace

### Python (rclpy)
- 用相對 topic 名稱
- Logger 用 `self.get_logger().info(..., throttle_duration_sec=1.0)`
- `try/except KeyboardInterrupt` 包 `rclpy.spin(node)`
- Subscriber 對感測器資料用 `qos_profile_sensor_data`
- 大量資料優先 NumPy 向量化
- 加 type hints：`def callback(self, msg: Twist):`
- 不要 `print()`，永遠用 logger

### Markdown
- 章節用 emoji 標題區分（📍 觀念、🕵️ 偵探、💻 程式、🚀 執行、🎯 總結、🌟 挑戰）
- 表格優先於 bullet list（資訊密度高）
- 程式碼區塊一律標語言（` ```cpp ` `​```python ` `​```bash ` ` ```cmake `）
- 警告用 `> ⚠️`、提示用 `> 💡`、注意用 `> ⚠️ 關鍵知識：`
- 引用其他章用相對路徑：`[Phase 03](../phase-03-xxx/)`
- C++ 讀者提示用 `<details><summary>` 折疊
- Python 引導小字用 `<sub>🐍 ...</sub>`

---

## ✏️ 寫作風格

- **直接對讀者說話**：用「你」，不要「我們」「使用者」
- **動詞開頭**：「打開終端機」「執行 colcon build」，避免「需要做的是」
- **具體 > 抽象**：寫「車子前進 3 秒後停下」，不寫「機器人會移動然後停止」
- **錯誤訊息要列原文**：讓讀者 grep 得到
- **時長標示**：每章 README 開頭可標「預計 XX 小時」，跟 ROADMAP 對齊
- **不要過度禮貌**：少用「希望這對你有幫助」「祝你學習順利」
- **不要用「點讚訂閱」式的結語**：「恭喜！你已經精通...」這種話最多用一次

---

## 🚫 反模式（過去踩過的坑，別再犯）

| 反模式 | 為什麼是錯的 | 正確做法 |
|--------|-------------|---------|
| Topic 寫死 `/originbot_1/cmd_vel` | 換環境就要改 code | 用 `cmd_vel` + 執行時 remap |
| Sub callback 跟 Service callback 共享 `bool` | 多執行緒下 race condition | `std::atomic<bool>` 或 mutex |
| Python README 從 C++ 版翻譯 | 沒提供 Python 開發者真正需要的內容 | 加上 `--symlink-install`、NumPy、logger 等 Python 特有 |
| 把所有筆記寫成單一大檔 | 不能跳讀、不能單獨分享 | 每章獨立資料夾 |
| 步驟只寫一種環境 | 另一邊讀者跑不起來 | 必寫 TheConstruct + 本機兩種 |
| 程式碼沒附「為什麼」 | 跟官方 tutorial 沒差 | 加「踩雷」「對比官方範例」段落 |
| 資料夾結構混亂（`code/`、`source/`、`my_pkg/`） | 跨章不一致 | 統一 `code/my_cpp_pkg/` 或 `python/my_py_pkg/` |
| 用「我」「我們」 | 不像教學文件 | 用「你」 |
| 寫「點讚分享訂閱」式結尾 | 讓人尷尬 | 直接連到下一章就好 |

---

## 🆕 進階寫作模式（從 Phase 17–22B 整理）

寫 Track A/B 大章節（多 package 整合 / Docker / SLAM/Nav2/MoveIt）時的新發現:

### 1. 多 package launch 整合用 IncludeLaunchDescription + TimerAction

```python
gazebo = IncludeLaunchDescription(
    PythonLaunchDescriptionSource(
        os.path.join(get_package_share_directory('my_gazebo_demo'),
                     'launch', 'headless_demo.launch.py')))

# delay 讓上游穩定再啟動下游
slam = TimerAction(period=5.0, actions=[
    Node(package='slam_toolbox', executable='async_slam_toolbox_node',
         parameters=[slam_yaml])])
```

兩個習慣:
- **Capstone 章節 launch 短**:每個 phase 把細節打包好,Capstone include 起來只剩 5 行
- **TimerAction 是必需的**:Gazebo 啟動慢,SLAM/Nav2 啟動晚一些避免 race

### 2. launch 內展開 xacro 必須 ParameterValue 包

```python
from launch_ros.parameter_descriptions import ParameterValue

robot_description = ParameterValue(
    Command(['xacro ', xacro_file]),
    value_type=str)              # 沒這個 launch 會把 URDF 當 YAML parse,炸
```

### 3. WSL 裡跑 demo 的最穩做法:`timeout NN ros2 launch ...`

WSL2 的 systemd-user-session 會回收 setsid/nohup detach 的 process,**background tool 的 SIGKILL exit 9 也常見**。所以:

```bash
# ❌ 不可靠
ros2 launch xxx.launch.py &
sleep 30
ros2 topic echo /foo

# ✅ 同步跑完
timeout 30 ros2 launch xxx.launch.py 2>&1 | grep -E '...'
```

寫教學 demo 設計**自己會 timeout 結束**的 launch:用 `TimerAction` + `ExecuteProcess` 帶超時。

### 4. Docker DDS 雙雷的標準解法

每個 docker-compose service 都加:

```yaml
network_mode: host                # 雷 1:DDS multicast 必需,bridge 收不到資料
ipc: shareable                    # 雷 2:跨 container SHM transport,BestEffort 才能傳
                                  # 第二個 service: ipc: service:<第一個>
environment:
  - ROS_DOMAIN_ID=99              # 跟 host 隔離
```

### 5. 多 description params 給獨立 Node(MoveIt 等)

獨立 Node 用 `MoveGroupInterface` 等 API 必須**自己**帶 robot_description / semantic / kinematics:

```python
plan_demo = Node(
    package='my_arm_moveit_demo', executable='plan_demo',
    parameters=[robot_description, robot_description_semantic, kinematics])
```

C++ 端 Node 必設 NodeOptions:

```cpp
auto node = std::make_shared<rclcpp::Node>("plan_demo",
  rclcpp::NodeOptions().automatically_declare_parameters_from_overrides(true));
```

不然讀不到 nested yaml 的 sub-key。

### 6. YAML 裡 array 必須型別一致

```yaml
# ❌ rcl 噴 'Sequence should be of same type'
process_noise_covariance: [0.05, 0, 0, ..., 0.06, ...]

# ✅ 全寫 float
process_noise_covariance: [0.05, 0.0, 0.0, ..., 0.06, ...]
```

### 7. WSL GPU 限制不要硬扛

SLAM/Nav2/MoveIt OMPL 在 WSL2 沒 GPU 都會慢/不出結果。**教學策略**:
- 結構驗證(launch 起來、lifecycle active、topic discovery 對)在 WSL 可達 → 寫進「驗證過」段
- 真實 demo(SLAM 出地圖、Nav2 真的跑、MoveIt execute)→ 留給雲端 ROSject(有 GPU)或實機
- README 內**老實寫**「WSL 結構驗證過,GPU demo 雲端跑」,不要假裝跑得起來

### 8. 「驗證過」段落必須是**真實 log 摘要**

不是「如果你照做應該會看到」,而是 copy/paste 自 timeout 命令的實際輸出:

```
[plan_demo] connected to group 'arm'
[named pose 'ready']           ✅ plan OK | points= 73 | duration=7.167s
[joint values target]          ✅ plan OK | points= 60 | duration=5.834s
```

### 9. Capstone 章節要**整合 + 加值**,不要只是 include 別的 phase

Capstone A 不只 include 前面的 launch,還寫了 `auto_navigator.cpp`(Action client + initialpose pub + 三 waypoint sequence)— 這個是這章獨有的價值。
Capstone Final 加值是 Multi-stage Dockerfile + .dockerignore 設計,把 sibling phase 串成可交付 image。

### 10. 雷區條目必須是**真實踩到的**,不是預想的

每個雷:
- 開頭寫**症狀**(error message 原文,讀者可 grep)
- 寫**原因**(為什麼會這樣)
- 寫**解**(具體 code/yaml 改動)

不要寫「可能會有以下問題」這種預測式雷區。Phase 24/Phase 21A/Phase 22A 等章雷區條目都是當下 build/run 踩到、然後修好的,所以特別有價值。

---

## 🔄 修改既有章節時

- **不要重寫**，用 Edit 工具做最小變更
- **改 ROADMAP 編號要連動**：README.md 的章節表、各章 README 的「下一步」連結都要更新
- **加新元素時不要破壞既有章節**：例如想加 Rust 版，只在新章節加 `rust/`，不要回頭改老章節

---

## 🆕 加新章節時的檢查清單

寫完一章按這個順序檢查：

1. [ ] 資料夾結構符合標準（`code/my_cpp_pkg/`、必要時 `python/my_py_pkg/`）
2. [ ] `package.xml` 與 `CMakeLists.txt` 完整可編譯
3. [ ] C++ code 用相對 topic 名稱
4. [ ] 主 README 含 TheConstruct + 本機 WSL2 兩種執行步驟
5. [ ] 主 README 末尾有「下一步」連結
6. [ ] 如果有 Python 版：`python/` 結構完整，README 含 C++ 對照折疊區、Python 特有提示段
7. [ ] 主 README 末尾加 `<sub>🐍 ...</sub>` 引導（如果有 Python 版）
8. [ ] 更新頂層 `README.md` 的章節表（含 🐍 Py 欄位）
9. [ ] 更新 `ROADMAP.md` 的狀態（⬜ → ✅）
10. [ ] 提交前自己讀一遍：能不能跳過前面所有章節，直接從這章開始學？

---

## 📚 參考資料夾

寫新章節前先看以下檔案找風格參考：

- 標準 C++ 章：[`phase-01-cloud-env-first-publisher/README.md`](phase-01-cloud-env-first-publisher/README.md)
- 含 QoS 與 LiDAR 處理：[`phase-03-subscriber-lidar-brake/README.md`](phase-03-subscriber-lidar-brake/README.md)
- 純觀念章（無 code）：[`phase-02-communication-concepts/README.md`](phase-02-communication-concepts/README.md)
- 帶 race condition 修復的章：[`phase-04-services-toggle/README.md`](phase-04-services-toggle/README.md)
- 標準 Python 章：[`phase-01-cloud-env-first-publisher/python/README.md`](phase-01-cloud-env-first-publisher/python/README.md)
- Python 含 NumPy 加速：[`phase-03-subscriber-lidar-brake/python/README.md`](phase-03-subscriber-lidar-brake/python/README.md)
