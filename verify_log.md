# Verify Log — 進階生態 5 章 colcon 驗證

> 用 [`scripts/verify_advanced_phases.sh`](scripts/verify_advanced_phases.sh) 跑 Phase 30/32/35/36/37 的 `colcon build` + `colcon test`,記錄每次跑的結果與發現的 bug。

---

## 2026-05-05:最終驗證 — 5 章全綠 ✅

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
