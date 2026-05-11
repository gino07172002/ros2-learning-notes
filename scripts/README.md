# scripts/

驗證跟維運用的 shell 腳本。

---

## 兩支驗證腳本的差別

### `verify_advanced_phases.sh` — 歷史紀錄版,只跑進階生態 5 章

跑進階生態 5 章(Phase 30/32/35/36/37)的 `colcon build` + `colcon test`,**這是 [verify_log.md](../verify_log.md) 內 2026-05-05 那輪驗證用的腳本**。

```bash
bash scripts/verify_advanced_phases.sh
bash scripts/verify_advanced_phases.sh --keep-build
```

- 產出檔案:`verify_log.md`(會被覆寫,改前先備份)
- 適合場景:**只想重跑進階生態 5 章驗證**(快、紀錄完整)

### `verify_phases.sh` — 通用版,可選 group 涵蓋更廣

延伸版本,可以選 group 跑(主線基礎 / Track / Capstone / 進階生態 / 進階支線 / 全部)。

```bash
bash scripts/verify_phases.sh                   # 預設:mainline-core(主線基礎章節)
bash scripts/verify_phases.sh advanced          # 跟 verify_advanced_phases.sh 等價
bash scripts/verify_phases.sh capstones         # Capstone 1 / A
bash scripts/verify_phases.sh mainline-tracks   # Track A/B + Part 4 章節
bash scripts/verify_phases.sh advanced-drafts   # advanced/ 文字草稿(預期會有 fail)
bash scripts/verify_phases.sh all               # 全部 — 慢
bash scripts/verify_phases.sh --keep-build all  # incremental build
```

- 產出檔案:`verify_phases_log.md`(覆寫)
- 適合場景:**重大改動後想全 repo 跑一輪**、或**驗 advanced/ 文字草稿是不是真的能 build**

### 為什麼留兩支?不合併?

`verify_advanced_phases.sh` 是 2026-05-05 那輪驗證的歷史紀錄(verify_log 內引用),動它會破壞那段故事可信度。`verify_phases.sh` 是擴大版的「未來主用」腳本。**兩支腳本都會用同樣的 `colcon build / test` 邏輯**,只是涵蓋範圍不同。

---

## 怎麼擴新章節進 verify_phases.sh

打開 `verify_phases.sh`,找到 `PHASES=(...)` 陣列,加一行:

```bash
"phase-XX-topic|pkg_dirname|0|mainline-core"
#  ^               ^               ^   ^
#  |               |               |   group:mainline-core / mainline-tracks /
#  |               |               |        capstones / advanced / advanced-drafts
#  |               |               has_gtest:0 沒有,1 有 colcon test
#  |               package.xml 內的 <name>(也是 code/ 下面的子資料夾名)
#  phase 資料夾名
```

**多 package 的章節**(例 Phase 08 my_robot_interfaces + my_cpp_pkg):
- 每個 package 一行
- 順序很重要:**被依賴的 package 排前面**(例:my_robot_interfaces 要在 my_cpp_pkg 前)

**advanced/ 路徑**:寫成 `advanced/<track>/<chapter>` 取代 `phase-XX`,腳本支援這種路徑。
