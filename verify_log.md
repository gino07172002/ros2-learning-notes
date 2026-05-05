# Verify Log — colcon / runtime 驗證紀錄

> 紀錄每次重新驗證的結果。
>
> - **進階生態 5 章 (Phase 30/32/35/36/37)** 用 [`scripts/verify_advanced_phases.sh`](scripts/verify_advanced_phases.sh) 跑
> - **其他章節**手動驗證(主線基礎、Phase 05 工具集等)
>
> 紀錄按時間反序(最新在上)。

---

## 2026-05-05(晚上):Phase 06 Parameters 完整 demo 驗證 ✅

### Build
- `colcon build --packages-select my_cpp_pkg` (Phase 06) **passed in 32.1s**
- install:`auto_brake_param` 執行檔 + `share/my_cpp_pkg/config/auto_brake_params.yaml` 部署到位

### Runtime — 5 個 Demo 全綠

| Demo | 章節指令 | 預期 | 實測 | 結果 |
|------|---------|------|------|------|
| **1 list** | `ros2 param list /auto_brake_param_node` | 看到 3 個自訂 + 內建 (use_sim_time / qos_overrides) | 三個自訂 + use_sim_time + 4 個 qos_overrides 全在 | ✅ |
| **1 get** | `ros2 param get /...node safe_distance` | `Double value is: 1.0` | 一致 | ✅ |
| **2 set** | `ros2 param set ... safe_distance 2.5` | `Set parameter successful` + log 印 `safe_distance -> 2.50` | 兩段都印出來,**on_set callback 攔截器運作** | ✅ |
| **3 攔截 over-range** | `ros2 param set ... max_speed 5.0` | `Setting parameter failed: max_speed must be in [0, 2.0]` | **錯誤訊息一字不差** | ✅ |
| **3 攔截負值** | `ros2 param set ... safe_distance -1.0` | `Setting parameter failed: safe_distance must be >= 0` | **錯誤訊息一字不差** | ✅ |
| **3 確認沒被改** | `ros2 param get ... safe_distance` | 還是 2.5(維持 Demo 2 設定) | 2.5 ✅ | ✅ |
| **4 YAML 載入** | `--params-file ...auto_brake_params.yaml` | 啟動 log 印 `safe_distance=0.80, max_speed=0.15, corridor_width=0.50` | **跟章節 line 217 預期 log 完全一致** | ✅ |
| **5 plugin 雷** | `rqt --list-plugins` | 看到 `rqt_reconfigure.param_plugin.ParamPlugin`,**沒**舊版的 `rqt_reconfigure.rqt_reconfigure.RqtReconfigure` | 完全符合 | ✅ |

### 沒驗到的(明說)
- ❌ **Demo 5 GUI 視窗實際畫面** — WSL 沒開 X11、AI 看不到視窗(章節已有截圖)
- ❌ **fake_lidar.py 觸發避障行為** — Demo 設計需要光達 mock 才能看到 BRAKING log,但**param 機制本身跟 lidar 無關**,Demo 1–4 不需 fake_lidar 也能完整驗

### 結論
**Phase 06 全章可信**:
- 所有章節指令 verbatim 可用
- on_set callback 攔截器(章節核心 takeaway)實測攔下兩種壞值 + 拒絕後保留原值
- YAML 載入機制完全符合預期
- Humble 雷區 1(rqt_reconfigure 新舊 plugin 名)寫對

---

## 2026-05-05(晚上):Phase 01 + Phase 05 手動驗證 ✅

### Phase 01 — Publisher build + executable 驗

| 項目 | 結果 |
|------|------|
| Source `/opt/ros/humble/setup.bash` | ✅ ROS_DISTRO=humble |
| Deploy `my_cpp_pkg` 到 `~/ros2_ws/src/` | ✅(Phase 01 沒撞到既有 phase01_pkg)|
| `colcon build --packages-select my_cpp_pkg` | ✅ **passed in 39.9s** |
| `~/ros2_ws/install/my_cpp_pkg/lib/my_cpp_pkg/auto_drive` | ✅ executable 存在 |
| 部署順序對(沒外部依賴) | ✅ 純 rclcpp + geometry_msgs |

**結論**:Phase 01 主線最基礎章 colcon build 過,**驗證 GETTING_STARTED 推薦的 7 章新手路徑第一站正確**。

### Phase 05 — Debug 工具集可用性驗

Phase 05 是純工具教學沒 code 要 build,改驗 4 個工具是否裝齊 + 章節指令正確。

| 工具 | 章節指令 | 驗證結果 |
|------|---------|----------|
| `rqt_graph` | line 45 `rqt_graph` | ✅ 在 `/opt/ros/humble/bin/rqt_graph` |
| `rqt` 主程式 | line 195 `rqt --standalone ...` | ✅ 在 `/opt/ros/humble/bin/rqt` |
| `ros2 bag` | line 125 `ros2 bag record ...` | ✅ subcommand 完整,實測 record 成功產出 metadata.yaml + .db3 |
| `ros2 bag info` | line 133 | ✅ 能讀剛產的 bag 並列 metadata |
| `rqt --standalone rqt_plot.plot.Plot` | line 195(Humble 雷的解法) | ✅ `rqt --list-plugins` 確認 entry point 名稱**完全正確** |
| `rqt --standalone rqt_console.console.Console` | line 201 | ✅ 同上,entry point 對 |
| 全套 rqt 套件 | — | ✅ 20 個 rqt_* package 全裝(rqt_action / bag / console / graph / plot / reconfigure / service_caller / topic 等)|

**驗證細節**:
```
ros2 pkg list | grep ^rqt → 20 個套件
which rqt_graph rqt → /opt/ros/humble/bin/{rqt_graph,rqt}
rqt --list-plugins | grep -iE 'plot|console' →
  rqt_console.console.Console
  rqt_plot.plot.Plot
  rqt_py_console.py_console.PyConsole
ros2 bag record /test_topic -o phase05_test_bag → 成功產出 db3 + metadata
ros2 bag info phase05_test_bag → 正確顯示 metadata
```

**結論**:Phase 05 README 內所有指令在 Humble 都可用,**特別是 line 192–201「Humble 沒有 rqt_plot 獨立執行檔」這條雷區的解法被驗證正確** — `rqt --standalone <plugin>` 的 plugin 名 `rqt_plot.plot.Plot` 跟 `rqt_console.console.Console` 真的存在於 plugin list。

> ⚠️ **沒驗到的**:rqt_graph / rqt_plot / rqt_console 三個 GUI 視窗實際畫面(WSL2 Headless 不開 X11,且 AI 看不到視窗)。這些章節的截圖驗證需要 gino 本機跑。

---

## 2026-05-05(早):進階生態 5 章 — 最終驗證全綠 ✅

### 結果

| Package | Phase | Build | Test |
|---------|-------|-------|------|
| my_bt_advanced | 30 | ✅ passed (2min 11s) | ✅ **6 + 1 ament**,0 errors, 0 failures |
| my_bag_demo | 32 | ✅ passed (4.9s) | — (no gtest by design) |
| my_foxglove_demo | 35 | ✅ passed (2.4s) | — (no gtest, pure config) |
| my_diag_demo | 36 | ✅ passed (1min 7s) | ✅ **4 + 1 ament**,0 errors, 0 failures |
| my_lifecycle_diag | 37 | ✅ passed (45.3s) | ✅ **5 + 1 ament**,0 errors, 0 failures |

(每章 colcon test summary 多 1 是 `ament_cmake_copyright` 自動加的 lint,不算我們寫的 gtest case。)

**寫的 gtest:** 15 個 case(Phase 30: 6 + 36: 4 + 37: 5),**全部 0 errors / 0 failures**。

---

## 2026-05-05:首輪驗證 — 抓到 2 個真實 bug

第一次跑 verify_advanced_phases.sh,**audit 預測完全成立** — 沒驗證的章節真的有 bug。

| Package | Build | 問題 |
|---------|-------|------|
| my_bt_advanced (30) | ✅ | OK,6 tests passed |
| my_bag_demo (32) | ✅ | OK |
| my_foxglove_demo (35) | ❌ | **Bug 2** — colcon 找不到 my_diag_demo install hook |
| my_diag_demo (36) | ✅ | OK,4 tests passed |
| my_lifecycle_diag (37) | ❌ | **Bug 1** — `current_state_label() const` 編譯錯 |

### Bug 1: Phase 37 — `get_current_state()` 不是 const,wrapper 不能標 const

```
healthy_lifecycle_node.cpp:96:27: error:
  passing 'const my_lifecycle_diag::HealthyLifecycleNode' as 'this' argument
  discards qualifiers [-fpermissive]
   96 |   return get_current_state().label();
```

**根因:** 我把 `current_state_label() const` 寫成 const method,但 `LifecycleNode::get_current_state()` 上游就**不是 const**(這是 rclcpp_lifecycle 的設計)。子類 const wrapper 沒辦法呼叫 non-const parent method。

**修法:** 拿掉 const(commit 同時改 header + cpp)。Header 內加註解講清楚雷的細節,給未來複製 lifecycle pattern 的人看。

```diff
- std::string current_state_label() const;
+ // ⚠️ LifecycleNode::get_current_state() 上游不是 const method,
+ //    所以這個 wrapper 也不能標 const(編譯期錯誤 -fpermissive)
+ std::string current_state_label();
```

### Bug 2: Phase 35 — `exec_depend my_diag_demo` 在獨立 build 時找不到 install hook

```
ERROR Failed to find /home/gino/ros2_ws/install/my_diag_demo/share/my_diag_demo/package.sh
```

**根因:** `package.xml` 裡放了 `<exec_depend>my_diag_demo</exec_depend>`,但 my_diag_demo 是純 demo launch 才需要,不是 build-time 真的依賴。當使用者只想 build my_foxglove_demo(沒裝 my_diag_demo)時,colcon hook resolver 找不到它的 install share。

**設計教訓:** 「每章獨立可學」是 repo 核心原則 — 加 hard depend 違反這個原則。launch 內已經有 `try/except` 軟性處理,移除 package.xml 的依賴宣告即可。

```diff
  <exec_depend>foxglove_bridge</exec_depend>
- <exec_depend>my_diag_demo</exec_depend>
+ <!-- my_diag_demo 是 demo launch 才需要 — 設成 hard depend 會強制
+      一起 build。launch 內已用 try/except 軟性處理。 -->
```

### 為什麼這兩個 bug 是好故事

audit 預測:「Phase 30+ 是**未驗證的速成版**,推測會在 colcon build 失敗。」
實際:5 章 colcon build 4 過 1 失,colcon test 3 過 0 失。**audit 比樂觀的我準。**

這驗證了「沒跑過的程式碼就是不可信的程式碼」 — 不論它看起來多像「同模式」。**之後新章節必須先過 verify 才升 ✅**。

---

## 怎麼自己跑

```bash
# WSL2 內
bash scripts/verify_advanced_phases.sh

# 留 build artifacts(下次 incremental build 較快):
bash scripts/verify_advanced_phases.sh --keep-build

# 結果:
#   verify_log.md            ← 這個檔(會被覆寫,記得先備份)
#   /tmp/verify_<TS>/*.log   ← 每章詳細輸出
```
