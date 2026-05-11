# Phase 25：CI/CD with GitHub Actions 🚀

> 把這個 repo 變成「**push code 自動跑測試 + 建 image**」的工作流。

**這章你將解鎖的業界 CI/CD 技能**：
- **打造全自動化流水線 (GitHub Actions)**：學會撰寫自動化腳本，讓 GitHub 在你每次 Push 程式碼時，自動拉起乾淨的 Ubuntu 容器，並在裡面執行 `colcon build` 與 `colcon test`，徹底消滅「只有在你的電腦上會動」的窘境。
- **矩陣式程式碼體檢 (Linter Matrix)**：配置平行運算的測試矩陣，讓系統同時使用 `cpplint`、`flake8` 等多種工具對你的 C++ 與 Python 程式碼進行嚴格的風格與品質審查。
- **自動產出交付物 (GHCR)**：建立自動打包流程，讓通過測試的程式碼自動被封裝成 Docker Image，並推送到 GitHub Container Registry (GHCR)，隨時準備好讓終端設備下載更新。

**前置準備**：
- [Phase 12 測試](../phase-12-testing/) — CI 測試的對象
- 對 GitHub 帳號和 repo 的基本操作

**產出目標**：[`example/.github/workflows/`](example/.github/workflows/) — 三個範例 workflow（ci.yml / lint.yml / docker.yml）+ Dockerfile

**環境**：💻 GitHub Actions（雲端）

---

## 🤔 為什麼 CI/CD 重要

寫了 Phase 12 測試卻沒 CI，就等於**你寫的測試根本沒人會跑**。人類總有忘記下測試指令的時候。

在專業的 ROS 開源專案 (如 Nav2、MoveIt、ros2_control) 或商業公司中，**你的 PR 如果沒有通過 CI，資深工程師連看都不會看一眼**。業界標準的自動化流程如下：

1. **觸發器 (Trigger)**：當開發者將程式碼 Push 到伺服器，或發起 Pull Request (PR) 時，GitHub 會自動喚醒雲端伺服器。
2. **平行審查 (Parallel Jobs)**：伺服器會同時分派三個任務：
   - **Job 1 (靜態分析)**：檢查程式碼有沒有排版錯誤、有沒有宣告未使用的變數。
   - **Job 2 (編譯與單元測試)**：實際編譯整個 Workspace，並執行所有的 GTest 與 PyTest。
   - **Job 3 (打包建置)**：驗證 Dockerfile 是否寫錯，能否成功建置出 Image。
3. **生死門 (Status Check)**：只有當這三個 Job 全數亮起綠燈 (✅) 時，Merge 按鈕才會解鎖。只要有任何一個環節失敗 (❌)，程式碼就會被無情地擋在主分支門外。

---

## 🏗️ 三個 Workflow

### 1. ci.yml — Build + Test

完整檔案見 [`example/.github/workflows/ci.yml`](example/.github/workflows/ci.yml)。

```yaml
name: ROS 2 CI

on:
  push:
    branches: [main]
  pull_request:
    branches: [main]

jobs:
  build_and_test:
    runs-on: ubuntu-22.04
    container:
      image: osrf/ros:humble-desktop      # ← 官方 ROS 2 image

    steps:
      - uses: actions/checkout@v4

      - name: Setup workspace
        run: |
          mkdir -p /tmp/ros2_ws/src
          cp -r examples/phase12_pkg /tmp/ros2_ws/src/

      - name: Install dependencies
        run: |
          source /opt/ros/humble/setup.bash
          rosdep update
          rosdep install --from-paths src --ignore-src -y

      - name: Build
        run: |
          source /opt/ros/humble/setup.bash
          colcon build

      - name: Test
        run: |
          source /opt/ros/humble/setup.bash
          colcon test
          colcon test-result --verbose          # ← 沒這行 fail 不會 fail

      - name: Upload results
        if: always()
        uses: actions/upload-artifact@v4
        with:
          name: test-results
          path: build/*/test_results/
```

**💡 劃重點**：
- **`container: osrf/ros:humble-desktop`** — 直接用官方 image，省去自己裝 ROS 2
- **`rosdep install --from-paths src`** — 自動讀 package.xml 內 `<depend>` 安裝
- **`colcon test-result --verbose`** — colcon test 即使 fail 也回傳 0，必須這行才能 fail CI
- **upload-artifact** — 失敗時下載測試報告分析

### 2. lint.yml — 程式碼風格檢查

```yaml
strategy:
  fail-fast: false                # 讓所有 linter 跑完，不要一個 fail 就停
  matrix:
    linter:
      - ament_cpplint
      - ament_uncrustify
      - ament_flake8
      - ament_xmllint

steps:
  - run: ${{ matrix.linter }} examples/
```

**矩陣 strategy** 同時跑 4 個 linter，比較快。每個 linter 是獨立 job，獨立 fail。

### 3. docker.yml — Build + Push Image

```yaml
on:
  push:
    branches: [main]
    tags: ['v*']        # v1.0.0 之類

jobs:
  build_image:
    permissions:
      packages: write    # ← 推 GHCR 必須

    steps:
      - uses: docker/login-action@v3
        with:
          registry: ghcr.io
          username: ${{ github.actor }}
          password: ${{ secrets.GITHUB_TOKEN }}     # ← 自動的

      - uses: docker/metadata-action@v5
        # 自動產出 tag：分支名、PR 號、語意版本

      - uses: docker/build-push-action@v5
        with:
          context: .
          push: ${{ github.event_name != 'pull_request' }}    # PR 不推
```

**業界用途**：
- merge 到 main → image 推到 GHCR → CD 系統自動部署到實機
- 每個 PR 也 build 一次（不推），確認 Dockerfile 沒壞

### 對應的 Dockerfile

[`example/Dockerfile`](example/Dockerfile) 用兩階段 build：

```dockerfile
# Stage 1: 用 desktop image 編譯
FROM osrf/ros:humble-desktop AS builder
COPY . src/
RUN colcon build --merge-install

# Stage 2: 只把 install/ 複製到小 image
FROM osrf/ros:humble-ros-core
COPY --from=builder /workspace/install /workspace/install
ENTRYPOINT ["/workspace/entrypoint.sh"]
```

**為什麼兩階段**：desktop image ~3GB，ros-core 只 ~500MB。production 部署不要帶開發工具。

---

## 🚀 怎麼把 CI 加到你的 repo

### Step 1：把 workflow 檔案複製進 repo

從你的 ros2-learning-notes 根目錄：
```bash
mkdir -p .github/workflows
cp phase-25-ci-cd/example/.github/workflows/*.yml .github/workflows/
```

### Step 2：調整 ci.yml 內的套件路徑

預設 ci.yml 抓 `examples/phase12_pkg`。改成你 repo 內的實際路徑：
```yaml
- name: Setup workspace
  run: |
    mkdir -p /tmp/ros2_ws/src
    cp -r phase-12-testing/code/my_cpp_pkg /tmp/ros2_ws/src/phase12_pkg
    sed -i 's|<name>my_cpp_pkg</name>|<name>phase12_pkg</name>|' /tmp/ros2_ws/src/phase12_pkg/package.xml
    sed -i 's|project(my_cpp_pkg)|project(phase12_pkg)|' /tmp/ros2_ws/src/phase12_pkg/CMakeLists.txt
```

### Step 3：commit + push

```bash
git add .github/workflows/
git commit -m "Add GitHub Actions CI workflows"
git push
```

打開 GitHub repo 的 **Actions** tab，會看到 workflow 開始跑。

---

## 🐛 常見雷

### 雷 1：`colcon test` pass 但實際有測試 fail
```yaml
# ❌
- run: colcon test                    # 即使測試 fail 也回傳 0

# ✅
- run: |
    colcon test
    colcon test-result --verbose       # 這行才會 fail
```

### 雷 2：rosdep update 在 docker 內 fail
```yaml
# 某些 image 沒預先 rosdep init
- run: |
    rosdep init || echo "already initialized"
    rosdep update
```

### 雷 3：忘記 `permissions: packages: write`
推 GHCR 需要這個 permission，預設是 read-only。

### 雷 4：Dockerfile 用 `apt-get update` 沒清 cache
```dockerfile
# ❌ 增加 image 體積
RUN apt-get update && apt-get install ...

# ✅
RUN apt-get update && apt-get install ... && \
    rm -rf /var/lib/apt/lists/*
```

### 雷 5：CI 在 self-hosted runner 反而慢
GitHub free tier 給 ubuntu-22.04 runner 很快。**不要為了「省」自己架 self-hosted**——除非你有 GPU 模擬需求。

### 雷 6：secrets 漏寫到 log
```yaml
# ❌ token 印到 log
- run: echo "Token is ${{ secrets.MY_TOKEN }}"

# GitHub 會自動 mask 已宣告的 secrets，但寫成 plain string 會洩漏
```

---

## 🎯 學到的關鍵概念

- **腳本架構的三位一體**：GitHub Actions 的 YAML 檔永遠由三個元素組成：定義何時跑的 `on` (觸發條件)、定義做什麼任務的 `jobs` (平行工作區)，以及定義具體指令的 `steps` (執行步驟)。
- **站在巨人的肩膀上 (`osrf/ros` Image)**：我們不需要在腳本裡痛苦地寫 `apt-get install ros-humble-...`，直接在 CI 指定使用官方維護的 Docker Image 作為底層容器，省下大把的編譯環境準備時間。
- **自動依賴解析 (`rosdep install`)**：這是 CI 腳本中最不可或缺的一行。它會去讀取你所有 `package.xml` 裡的 `<depend>` 標籤，然後自動幫你 `apt install` 缺少的函式庫。
- **測試框架的隱藏陷阱 (`colcon test-result`)**：千萬記住，`colcon test` 就算遇到報錯，它的 Exit Code 依然會是 `0` (成功)。你必須強制加上 `colcon test-result --verbose`，它才會在讀到錯誤報告時吐出非 `0` 值，讓 CI 正確亮紅燈。
- **為頻寬著想 (Multi-stage Build)**：在 CI 打包映像檔時，透過多階段建置將高達幾 GB 的編譯工具鍊拋棄，只把乾淨的二進位檔案推上雲端，這對實機部署的速度有決定性的影響。
- **開源界的寶庫 (GHCR)**：不再依賴限制多多的 Docker Hub。GitHub 提供的 GHCR 讓你能免費地將 Image 與原始碼儲存在同一個生態系裡。
- **平行加速的魔法 (Matrix Strategy)**：要同時測 Python、C++ 與 XML 的風格，不用寫三個 Job。透過 Matrix 設定，GitHub 會自動幫你開好幾台機器，同時執行所有的檢查工具，大幅縮減 CI 執行時間。

---

## 🌟 進階挑戰

1. **加 cache**：`actions/cache@v4` 把 `/tmp/ros2_ws/install` 緩存，下次 build 快 5x
2. **Codecov 整合**：跑 `colcon test --enable-coverage`，把報告上傳 codecov.io
3. **多 ROS 版本 matrix**：同時測 humble + iron + jazzy
4. **release-please**：merge 到 main 自動發版本號 + 寫 release notes

---

## 👣 下一步去哪？

- Phase 24 — Docker：本章用了 Docker，那章深入講 image 最佳實踐
- Phase 27 — 部署實機（待完成）：CI 產出的 image 怎麼上 Pi / Jetson

---

## 📁 完整檔案結構

```
phase-25-ci-cd/
├── README.md
└── example/
    ├── .github/workflows/
    │   ├── ci.yml           ← Build + Test
    │   ├── lint.yml         ← 多 linter matrix
    │   └── docker.yml       ← Build + Push GHCR
    └── Dockerfile           ← 多階段 build
```

> 注意：`example/` 是給你**複製到自己 repo 根目錄**用的範例，不是要 build 的 ROS 套件。
