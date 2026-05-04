# 🤝 AI Handoff — 給接手的 session

> 給未來協助維護本 repo 的 AI session（或我自己回來重新接手）。讀完這份你應該能立刻接上現有節奏，不會迷失方向或重做已完成的工作。

---

## 你是誰、跟使用者的關係

- **使用者**：gino（gino.tsai@goodlinker.io）。在 goodlinker 工作，正在學 ROS 2，學完想把這份做成 GitHub portfolio。
- **使用者背景**：寫過 C++ 和 Python，做過 IoT / MQTT 專案，沒寫過 ROS。所以解釋 ROS 概念時可以用 MQTT/gRPC/HTTP 對照。
- **語言**：使用者用繁體中文溝通，所有 README/code 註解都是繁中。
- **個性與偏好**：直接、不要長篇大論。簡短回答比啰嗦的解釋好。常常一句「好」「衝吧」就授權你動手。
- **工作模式**：常常開 auto mode 讓 agent 自己跑。但**遇到要殺 process、寫敏感資料、跨環境破壞性操作時還是要先問**。

---

## 這個 repo 是什麼

**ROS 2 Humble 學習筆記**（C++ 為主，少數 Python 對照），目標是寫成可單獨展示的 GitHub portfolio。

GitHub: https://github.com/gino07172002/ros2-learning-notes

**核心結構**：
- 6 個 Part（學習層次分組）
- 每個 Part 含多個 Phase（章節）
- 每個 Phase 是獨立資料夾，含 `README.md` + `code/` + `images/`
- **每章可單獨閱讀、單獨編譯、單獨執行**——這是設計核心原則

---

## 進度地圖（讀完這個就知道該做什麼）

### ✅ 完成的章節（37 章 — 2026-05-04 教程化大改造)

```
Part 1: 通訊基礎
  ✅ phase-01-cloud-env-first-publisher  (含 Python 對照)
  ✅ phase-02-communication-concepts     (純觀念)
  ✅ phase-03-subscriber-lidar-brake     (含 Python 對照)
  ✅ phase-04-services-toggle

Part 2: 工具與治理
  ✅ phase-05-debug-tools
  ✅ phase-06-parameters
  ✅ phase-07-mini-capstone-1            (整合 P3+P4+P6)

Part 3: 系統設計
  ✅ phase-08-custom-interfaces          (定義 my_robot_interfaces)
  ✅ phase-09-executors-lifecycle-composition  (加閱讀路徑指南)
  ✅ phase-10-launch-files-basics
  ✅ phase-11-launch-files-advanced
  ✅ phase-12-testing
  ✅ phase-13-actions-advanced
  ✅ phase-14-capstone-1                 (Capstone 1: ApproachController)

Part 4: 機器人形體
  ✅ phase-15-urdf                       (加 Part 3→4 序言 + URDF/SDF/RPY 速查)
  ✅ phase-16-tf2
  ✅ phase-17-gazebo                     (headless + ☁️ 雲端 GUI 完整步驟)
  ✅ phase-18-ros2-control               (mock_components,☁️💻 雙環境)
  ✅ phase-19-pluginlib
  ✅ phase-20-multi-machine              (Docker 模擬多機 + Discovery Server)

Part 5: 領域應用
  Track A (Mobile):
    ✅ phase-20A-odometry-ekf            (加 Covariance/EKF 觀念速成)
    ✅ phase-21A-slam-toolbox            (加 SLAM 觀念速成 + ☁️ 雲端步驟)
    ✅ phase-22A-nav2-basics             (加 Nav2 名詞速查 + ☁️ 雲端步驟)
    ✅ phase-23A-nav2-bt-plugin          (4 個 gtest 全過)
    ✅ phase-30-nav2-bt-advanced         (4 種 BT node 全集 + 6 gtest)
    ✅ phase-CapstoneA-mobile            (auto_navigator 整合 + ☁️ 雲端步驟)
  Track B (Arm):
    ✅ phase-20B-arm-urdf                (xacro + SRDF)
    ⏸ phase-21B-moveit-setup-assistant  (草稿:12 步驟+5 雷,等本機截圖實做)
    ✅ phase-22B-moveit-cpp              (MoveGroupInterface 4 種 plan target)
    ⬜ phase-23B-pick-and-place          (視覺主導)
    ⬜ Capstone B                         (視覺主導)

Part 6: 生產化部署
  ✅ phase-24-docker                     (Capstone 1 docker 化)
  ✅ phase-25-ci-cd                      (GitHub Actions matrix)
  ✅ phase-26-dds-qos                    (☁️💻 雙環境)
  ✅ phase-32-rosbag2-advanced           (含 Python 對照)
  ✅ phase-35-foxglove-bridge            (Foxglove + Phase 36 demo)
  ✅ phase-36-diagnostics-watchdog       (4 gtest)
  ✅ phase-37-lifecycle-diagnostics      (5 gtest)
  ✅ phase-Capstone-Final                (Capstone A docker 化,1.26GB image)
  ⬜ phase-27-real-hardware              (沒做:使用者沒 Pi/Jetson)
```

### 📚 教程化文件層(2026-05-04 加)

```
✅ GETTING_STARTED.md                    新手入口:MQTT 對照、7 章新手路徑、觀念地圖
✅ README.md                             加「你是誰」5 列分流表 + 全 32 章「為什麼學這章」欄
✅ ROADMAP.md                            環境標記跟 SETUP 對照表同步
✅ SETUP.md                              新增「各章雲端可用性對照表」+ 免費資源指引
```

### ⏸/⬜ 還沒做的章節(什麼時候該做)

| 章節 | 現況 | 何時動手 |
|------|------|---------|
| **Phase 21B MoveIt Setup Assistant** | 草稿已完(12 步驟 + 5 雷),`images/` 留空 | 使用者本機跑 GUI 截 6 張圖補上 |
| **Phase 23B Pick & Place** | 視覺主導 | 使用者本機 + Gazebo + 抓物件 demo |
| **Capstone B 機械手臂** | 視覺主導 | 同上,等使用者要做 Track B 完整 demo |
| **Phase 27 部署實機** | 需要硬體 | 使用者真的買 Pi/Jetson 才有意義 |

---

## 重要慣例（**必讀**，違反會破壞 repo 一致性）

### 1. 章節編號 ≠ 學習順序
- Phase 14 是 Capstone 1(不是 Phase 17)
- Part 5 用「**字尾 A/B 區分 Track**」:`20A/21A/22A/23A` = Track A Mobile,`20B/22B` = Track B Arm
- Capstone 用「**字尾 -mobile / -Final**」:`phase-CapstoneA-mobile` / `phase-Capstone-Final`
- 編號是「**位置標記**」,學習順序看 README/ROADMAP 的 Part 結構

### 2. Code 套件命名
- 每章 `code/` 下叫 `my_cpp_pkg`（**刻意重名**，每章獨立）
- 部署到 WSL workspace 時改成 `phase{XX}_pkg` 避免衝突
- **不要試圖讓多章共用同一個 my_cpp_pkg**——違反「每章獨立」原則
- **唯一例外**：`my_robot_interfaces`（Phase 08 定義，Phase 13/14 重用）和 `my_robot_description`（Phase 15 定義，可被 Phase 17/18 重用）

### 3. README 結構（**每章必有**）
```
# Phase XX: 標題

**學完你會**: 具體可驗證的能力
**前置**: 明確列出依賴章節
**產出**: 連結到 code/ 子資料夾
**環境**: ☁️ TheConstructSim + 💻 本機 WSL 雙環境

## 為什麼 / 觀念
## 程式碼亮點（含註解）
## Demo 流程（雙環境分流）
## 常見雷（5–8 條，從踩過的真實雷紀錄）
## 學到的關鍵概念
## 進階挑戰
## 下一步
## 完整檔案結構
```

詳細模板見 [`AUTHORING_GUIDE.md`](AUTHORING_GUIDE.md)。

### 4. 雙環境支援
**每章 Demo 必須支援兩種環境**：
- ☁️ **TheConstructSim** 雲端 ROSject
- 💻 **本機 WSL2 + Ubuntu 22.04 + ROS 2 Humble**

差異只能在 Step 1 部署（git clone vs cp）和 topic remap，不能在邏輯。

### 5. 「驗證過」是嚴肅承諾
- README 內所有 Demo log（標 "驗證過" 或 "實測"）必須是**真實在 WSL 跑出來的輸出**
- 不要編造預期 log。如果改了 code 必須重跑驗證
- 失敗就修到能跑，不要假裝過

### 6. Commit 訊息格式
- 標題：`Add Phase XX: 主題` 或 `Phase XX: 改動描述`
- Body：列具體改了什麼、驗證過什麼（含 log 摘要）
- 結尾固定加：
  ```
  Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
  ```

### 7. **嚴格禁止**
- ❌ Topic 名寫死（例：`/originbot_1/cmd_vel`）—— 一律用相對名 + remap
- ❌ Python 對照版只是 C++ 翻譯——必須有 Python 特有內容（`--symlink-install`、NumPy、KeyboardInterrupt 等）
- ❌ 中文檔名（`螢幕擷取畫面.png`）——一律 kebab-case 英文
- ❌ 直接 push 不經過 commit（rebase / amend 只在使用者要求時做）
- ❌ 跳過驗證直接寫 README——會被使用者抓到

---

## 你的工具鏈（WSL2 環境細節）

### 已裝好的東西
- Ubuntu 22.04 LTS
- ROS 2 Humble Desktop（386 個套件）
- Gazebo Classic 11
- turtlebot3 全套（Phase 17 之後若需要）
- Nav2 全套（Track A 之後若需要）
- joint_state_publisher（Phase 15 用）
- 各 phase 的 ros2_control / rclcpp_lifecycle 等都已裝

### 工作區結構
```
/home/gino/ros2_ws/
├── src/                # 各 phase 套件（重命名後的）部署位置
├── install/            # colcon build 產物
├── build/
└── log/
```

### `~/.bashrc` 已 source 兩個環境
```bash
source /opt/ros/humble/setup.bash
source /home/gino/ros2_ws/install/setup.bash
export TURTLEBOT3_MODEL=burger
```

### 工具腳本
- **`/home/gino/run_node.sh`**（**用這個，不是 /tmp**）：source 環境後 exec 給定命令的 wrapper。background detach 必用。
- **`/home/gino/fake_lidar.py`**：發 PointCloud2 給 Phase 03/04/07/08/13/14 demo 用
- 兩個都在 home 目錄，重啟 WSL 不會掉

### 你跑命令的常見模式

**前景跑（短指令）**：
```bash
wsl -d Ubuntu -u gino -e bash -c "source /opt/ros/humble/setup.bash && source ~/ros2_ws/install/setup.bash && ros2 ..."
```

**背景跑（長執行的 demo）**：
```bash
wsl -d Ubuntu -u gino -e bash -c "setsid ~/run_node.sh ros2 launch ... > /tmp/xxx.log 2>&1 < /dev/null & disown ; sleep 5 ; ..."
```

**ros2 daemon 卡了的話**：
```bash
wsl ... "ros2 daemon stop ; sleep 1 ; ros2 daemon start"
```

---

## 寫新章節的標準流程

照這 8 步走，不要跳：

### 1. 設計
- 跟使用者討論章節範圍（或 auto mode 自己決定）
- 確認哪些是「必須教的」「可選的」「太重不要做」
- 想清楚 demo 故事——這章要展示什麼戲劇性效果

### 2. 建資料夾
```bash
mkdir -p phase-XX-topic/code/<pkg_name>/{src,launch,config} phase-XX-topic/images
```
名稱用 `kebab-case`，跟 ROADMAP 編號對齊。

### 3. 寫 code
- 套件叫 `my_cpp_pkg`（部署時改名）
- `package.xml` + `CMakeLists.txt` + `src/*.cpp`
- 註解：給「會 C++/IoT 但不會 ROS」的讀者看，重點解釋 ROS 概念與設計選擇

### 4. WSL 部署 + build 驗證
```bash
wsl -d Ubuntu -u gino -e bash -c "
  rm -rf ~/ros2_ws/src/phaseXX_pkg ;
  cp -r /mnt/d/ros_learn/ros2-learning-notes/phase-XX-xxx/code/my_cpp_pkg ~/ros2_ws/src/phaseXX_pkg &&
  sed -i 's|<name>my_cpp_pkg</name>|<name>phaseXX_pkg</name>|' ~/ros2_ws/src/phaseXX_pkg/package.xml &&
  sed -i 's|project(my_cpp_pkg)|project(phaseXX_pkg)|' ~/ros2_ws/src/phaseXX_pkg/CMakeLists.txt &&
  source /opt/ros/humble/setup.bash &&
  cd ~/ros2_ws &&
  colcon build --packages-select phaseXX_pkg 2>&1 | tail -5
"
```

**編譯失敗就修到過**。常見雷：
- `std::atomic<double>` 直接傳 printf → 改 `.load()`
- 漏 `find_package(rclcpp_action REQUIRED)` 之類
- pluginlib 漏 `member_of_group` tag

### 5. WSL 跑 demo 驗證
真的跑、真的看 log。把預期輸出複製到 README 的「驗證過」段落。失敗就 debug。

### 6. 寫 README
照模板（見 `AUTHORING_GUIDE.md`）。**「驗證過」段落必須真實**。常見雷至少 5 條。

### 7. 更新索引
- `README.md` 的章節表（在 Part 標題下）
- `ROADMAP.md` 的對應 row（⬜ → ✅）

### 8. Commit + Push
```bash
git add phase-XX-xxx/ README.md ROADMAP.md
git commit -m "$(cat <<'EOF'
Add Phase XX: 標題

具體做了什麼...

Verified live:
- Demo 1: ...
- Demo 2: ...

Documents N footguns including ...

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
git push origin main
```

---

## 過去踩過的真實雷（**不要再踩**）

### Phase 07 atomic copy ctor
```cpp
// ❌ atomic<double> 不能 implicit copy
RCLCPP_INFO(get_logger(), "speed=%.2f", max_speed_);

// ✅
RCLCPP_INFO(get_logger(), "speed=%.2f", max_speed_.load());
```

### Phase 08 rosidl 套件名衝突
- 套件叫 `my_robot_interfaces`，**Phase 13 必須擴充同一個套件**（加 Countdown.action）而非新建一個

### Phase 09 callback group 預設
```cpp
// MultiThreadedExecutor 對同 Node 的 callback 預設是 MutuallyExclusive
// 必須建 ReentrantCallbackGroup 並手動指定，並行才會生效
auto group = create_callback_group(rclcpp::CallbackGroupType::Reentrant);
slow_timer_ = create_wall_timer(5s, slow_cb, group);
```

### Phase 11 / 18 lifecycle event handler
```python
# ❌ EmitEvent without lifecycle_node_matcher
EmitEvent(event=ChangeState(transition_id=...))

# ✅
EmitEvent(event=ChangeState(
    lifecycle_node_matcher=lambda action: True,
    transition_id=...,
))
```

### Phase 12 colcon test 假成功
```bash
# ❌ colcon test 即使測試 fail 也回傳 0
colcon test

# ✅
colcon test
colcon test-result --verbose    # ← 這行才真的會 fail
```

### Phase 13 rclcpp_action segfault
- Humble 已知 issue：action client shutdown 時可能 segfault
- 不影響功能（result 已正確收到）
- README 標註「不影響 demo」即可，不要試圖修

### Phase 19 pluginlib 三檔對齊
```
package.xml <export><base_pkg plugin="${prefix}/plugins.xml" />
plugins.xml: <library path="lib_name">
CMakeLists.txt: pluginlib_export_plugin_description_file(base_pkg plugins.xml)
```
三檔的 base_pkg 名稱必須完全一致。

### Phase 26 QoS 兼容方向
- Reliable pub + Best Effort sub: ✅ 兼容（Reliable 可降級）
- Best Effort pub + Reliable sub: ❌ 不兼容
- 反過來測會看到收到訊息「以為驗證了」其實沒驗到不兼容

### /tmp 在 WSL 會被 cleanup
- background script 寫到 `/tmp/run_node.sh` 會在某些操作之間消失
- **改用 `~/run_node.sh`**

### ros2 daemon stale state
- 大量 SIGKILL 之後 `ros2 node list` 卡住或丟 `!rclpy.ok()`
- 解：`ros2 daemon stop ; ros2 daemon start`

### Phase 17/21A/22A WSL 沒 GPU 的硬上限
- SLAM(slam_toolbox)/AMCL/MoveIt OMPL 在 WSL2 沒 GPU 都會慢:
  - **SLAM**: scan timestamp earlier than transform cache + queue full,/map 永遠不出來
  - **AMCL**: 同樣 scan timestamp 問題,localization 不收斂
  - **MoveIt**: plan 還能跑(只 OMPL CPU),但 trajectory execute 假 controller 不動
- **教學策略**: 結構驗證(launch 起來、lifecycle active)在 WSL 可達;真實 demo 在雲端 ROSject(有 GPU)或實機
- 不要花時間 tune yaml 試圖在 WSL 跑出真 SLAM/Nav2,會無底洞

### WSL2 background process 跟 wsl 命令的 lifetime
- `wsl -d Ubuntu -e bash -c "<launch> &"` 結束時,setsid/nohup detach 出去的 process 仍可能被 wsl session manager 回收
- 「同步 timeout 命令」(`timeout 30 ros2 launch ...`)是最穩跑驗證的方式
- background tool(`run_in_background=true`)更不穩,經常 SIGKILL exit 9
- 教學 demo 設計成「自己會 timeout 結束」的 launch(用 `TimerAction` + `ExecuteProcess` 帶超時)

### Phase 22A/Capstone Final base_link vs base_footprint
- Nav2 預設 `nav2_params.yaml` 內 `robot_base_frame: base_link`,但 turtlebot3 SDF root 是 `base_footprint`
- 不改的話 local_costmap 永遠 `Timed out waiting for transform from base_link to odom`
- 解:`sed -i 's|robot_base_frame: base_link|robot_base_frame: base_footprint|g'`

### Phase 22B MoveIt 三份 description params
- 獨立 Node 用 `MoveGroupInterface` 必須**自己**帶上 `robot_description` / `robot_description_semantic` / `robot_description_kinematics`
- 不會自動從 move_group 共享 — 每個 Node 有自己的 parameter namespace
- 沒帶會炸 `Unable to parse SRDF` / `Unable to construct robot model`
- 還要 `NodeOptions().automatically_declare_parameters_from_overrides(true)` 才讀得到 nested yaml

### Phase 24 Docker DDS 雙雷
- 雷 A:bridge network → topic discovery 過,但 echo 收不到資料(multicast 被擋)
  - 解:`network_mode: host`
- 雷 B:host network 還是收不到 BestEffort sensor data
  - 解:`ipc: shareable` + `ipc: service:<另一服務>`(共享 IPC namespace,SHM transport 才打得通)

### Phase 17 URDF vs SDF
- `turtlebot3_description/urdf/turtlebot3_burger.urdf` **沒 `<gazebo>` 標籤**,spawn 後沒 sensor topic
- 必須用 `turtlebot3_gazebo/models/turtlebot3_burger/model.sdf`(完整 ros plugin)
- 設計:robot_state_publisher 讀 URDF(發 TF),spawn_entity 讀 SDF(進物理引擎)

### Phase 17 gazebo.launch.py vs gzserver.launch.py
- `gzserver.launch.py` 預設不載 `libgazebo_ros_factory.so` → `/spawn_entity` service 不存在
- 用 `gazebo.launch.py` 自帶完整 plugin set + 可加 `gui:=false` headless

### Phase 20A robot_localization 的 yaml 全 float
- `process_noise_covariance` 是 225 個元素 array,YAML parser 要求**所有 element 同型別**
- 寫 `[0.05, 0, 0, ...]` 會炸 `Sequence should be of same type, integer do not belong`
- 全部寫 `0.0` / `0.05` 等帶小數點的 float

### Phase 20A 缺 imu_link → base_link TF 的無聲失敗
- EKF 訂 `/imu/data` 看到 `frame_id: imu_link`,要把它轉到 `base_link`(EKF base_frame)
- 沒 TF 就**默默丟掉所有 IMU 訊息**,沒 error 沒 warning,只看到 EKF 不轉彎
- 解:launch 加 `static_transform_publisher base_link → imu_link`(實機要設真實 offset)

---

## 跟使用者溝通的眉角

### 使用者喜歡的
- 直接動手（auto mode 下不要問太多）
- 截圖前明確說「截哪個視窗、存什麼檔名」
- 進度報告用列表（清楚的 ✅ / ⬜）
- 重要的雷用粗體與 `⚠️`

### 使用者不喜歡的
- 廢話開場白（「好的，讓我來..」）
- 重複確認（除非真的有破壞性操作）
- 寫了 code 但沒驗證就交付
- 一次塞太多東西過來——「我等不及」也要切片做

### 使用者的口頭禪（看到就知道意思）
- 「好」「衝吧」「來」 = 授權執行
- 「你看看」 = 想看你判斷
- 「我等不及了」 = 全速 auto，不要再問

### 截圖流程
- 使用者用 Win+Shift+S 截圖，存到 phase-XX/images/
- 預設檔名是「螢幕擷取畫面 YYYY-MM-DD HHMMSS.png」
- **你要做**：偵測到中文檔名 → 立刻 mv 改成英文 kebab-case → 寫進 README

---

## 重要文件清單（先讀完這幾個再動手）

| 檔案 | 用途 |
|------|------|
| **HANDOFF.md** | 你正在讀這個 |
| [README.md](README.md) | repo 入口、章節表（含「你是誰」6 列分流) |
| [GETTING_STARTED.md](GETTING_STARTED.md) | 新手入口(MQTT 對照、7 章新手路徑、觀念地圖) |
| [DESIGN_NOTES.md](DESIGN_NOTES.md) | 「ROS 2 為什麼這樣設計」深挖,提煉 library 設計通則(目前 1 篇 + 5 個待寫主題) |
| [ROADMAP.md](ROADMAP.md) | 完整學習路徑、各章狀態 |
| [SETUP.md](SETUP.md) | 雙環境設定 + 各章雲端可用性對照表(32 章逐一標 ☁️/🟡/🚫) |
| [AUTHORING_GUIDE.md](AUTHORING_GUIDE.md) | 章節寫作模板（最重要！）|
| [PORTFOLIO.md](PORTFOLIO.md) | 履歷友善版,給招聘方/Code reviewer |

---

## 範例：一個完整 session 開頭應該怎麼做

```
1. 讀 HANDOFF.md（你在讀）
2. 讀 ROADMAP.md，確認哪些章節 ✅、哪些 ⬜
3. 跟使用者說「我看到目前到 X，下一步可以做 Y / Z / W，你想做哪個？」
   或在 auto mode 下：「我接著做 Y」
4. 動手前確認 wsl 環境健全：
   wsl -d Ubuntu -u gino -e bash -c "source /opt/ros/humble/setup.bash && ros2 pkg list | wc -l"
   # 預期至少 386
5. 動手前確認 git status 乾淨：
   cd "d:/ros_learn/ros2-learning-notes" && git status
6. 開始照「寫新章節的標準流程」8 步走
```

---

## 一個禁忌：不要回頭改既有章節結構

如果某章已經 ✅，**不要為了「讓它更好」就大改架構**。除非：
- 使用者明確要求
- 發現實質錯誤（code 不能 build、log 是假的）
- 影響後續章節

小修補（typo、補圖、補一條雷）OK。重排步驟順序、改 demo 故事、換套件名 = NG。

---

## 給未來 AI 的最後一句話

這個 repo 已經有 20 個 phase、約 2 萬行 markdown + code，是 gino 從零開始學 ROS 2 的真實旅程記錄。**你的工作是延續這個旅程，不是把它變成你想像中的完美版本**。

寫每一章前問自己：
1. 這對「會 C++/IoT 但不會 ROS」的 gino 有用嗎？
2. 我寫的 demo log 真的是 WSL 跑出來的嗎？
3. 我有沒有讓他能照著跑出我說的結果？

三個都 ✅ 才能 commit。

衝。
