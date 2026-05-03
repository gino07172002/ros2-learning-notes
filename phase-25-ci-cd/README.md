# Phase 25：CI/CD with GitHub Actions

> 把這個 repo 變成「**push code 自動跑測試 + 建 image**」的工作流。

**學完你會**：寫 GitHub Actions workflow 在容器裡跑 colcon build + colcon test、設多 linter matrix、build Docker image 並推到 GHCR。

**前置**：
- [Phase 12 測試](../phase-12-testing/) — CI 測試的對象
- 對 GitHub 帳號和 repo 的基本操作

**產出**：[`example/.github/workflows/`](example/.github/workflows/) — 三個範例 workflow（ci.yml / lint.yml / docker.yml）+ Dockerfile

**環境**：💻 GitHub Actions（雲端）

---

## 為什麼 CI/CD 重要

寫了 Phase 12 測試卻沒 CI = **沒人會跑那些測試**。

業界 ROS 專案 push code 流程：

```
git push
   │
   ▼
GitHub Actions trigger
   │
   ├─ Job 1: lint (cpplint, flake8, xmllint)
   ├─ Job 2: build + colcon test
   └─ Job 3: build Docker image
   │
   ▼
全部 pass → PR 顯示 ✅，可以 merge
任一 fail → PR 顯示 ❌，merge 鎖住
```

**Nav2、MoveIt、ros2_control 都這樣做**——你的 PR 沒過 CI 連看的人都沒有。

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

**重點**：
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

- **GitHub Actions 三大概念**：`on` 觸發 + `jobs` 工作 + `steps` 步驟
- **官方 osrf/ros image** 省去裝 ROS 2 的麻煩
- **`rosdep install`** 自動處理 package.xml 內的依賴
- **`colcon test-result`** 才會讓 CI fail
- **多階段 Docker build** 縮減 production image
- **GHCR (ghcr.io)** 免費的 GitHub container registry
- **Matrix strategy** 同時跑多個 linter

---

## 🌟 進階挑戰

1. **加 cache**：`actions/cache@v4` 把 `/tmp/ros2_ws/install` 緩存，下次 build 快 5x
2. **Codecov 整合**：跑 `colcon test --enable-coverage`，把報告上傳 codecov.io
3. **多 ROS 版本 matrix**：同時測 humble + iron + jazzy
4. **release-please**：merge 到 main 自動發版本號 + 寫 release notes

---

## 下一步

- Phase 24 — Docker（待完成）：本章用了 Docker，那章深入講 image 最佳實踐
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
