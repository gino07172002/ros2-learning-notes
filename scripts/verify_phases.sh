#!/usr/bin/env bash
# verify_phases.sh
#
# 通用版驗證腳本:可選擇 group 跑(主線 / Capstone / 進階生態 / 進階支線 / all)。
#
# 跟 verify_advanced_phases.sh 的差異:
#   - 那支腳本固定跑 advanced 5 章,是 2026-05-05 那輪驗證的歷史紀錄
#   - 這支腳本 group 可選,涵蓋更廣,適合定期重跑全 repo
#
# 跑法:
#   bash scripts/verify_phases.sh                   # 預設跑 mainline-core(風險低、快)
#   bash scripts/verify_phases.sh advanced          # 進階生態 5 章(等同 verify_advanced_phases.sh)
#   bash scripts/verify_phases.sh capstones         # 跑 Capstone 1 / A / Final
#   bash scripts/verify_phases.sh mainline-tracks   # Track A/B + Part 4 章節
#   bash scripts/verify_phases.sh advanced-drafts   # advanced/ 文字草稿(預期會 fail!)
#   bash scripts/verify_phases.sh all               # 全部 — 慢,留給確認重大改動後跑一輪
#   bash scripts/verify_phases.sh --keep-build all  # 不清舊 build(incremental)
#
# 預期輸出:
#   verify_phases_log.md    # 摘要表格(覆寫,跑前先備份重要紀錄)
#   /tmp/verify_phases_<TS>/<pkg>.log   # 每章詳細 log

set -o pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WS_SRC="${HOME}/ros2_ws/src"
TS="$(date +%Y%m%d_%H%M%S)"
LOG_DIR="/tmp/verify_phases_${TS}"
SUMMARY="${REPO_ROOT}/verify_phases_log.md"

# === 章節定義 ===
# 格式: phase_dir | pkg_dirname | has_gtest | group
# group:mainline-core / mainline-tracks / capstones / advanced / advanced-drafts
PHASES=(
    # --- mainline-core(主線基礎,Phase 01–13)---
    "phase-01-cloud-env-first-publisher|my_cpp_pkg|0|mainline-core"
    "phase-03-subscriber-lidar-brake|my_cpp_pkg|0|mainline-core"
    "phase-04-services-toggle|my_cpp_pkg|0|mainline-core"
    "phase-06-parameters|my_cpp_pkg|0|mainline-core"
    "phase-07-mini-capstone-1|my_cpp_pkg|0|mainline-core"
    "phase-08-custom-interfaces|my_robot_interfaces|0|mainline-core"
    "phase-08-custom-interfaces|my_cpp_pkg|0|mainline-core"
    "phase-09-executors-lifecycle-composition|my_cpp_pkg|0|mainline-core"
    "phase-10-launch-files-basics|my_cpp_pkg|0|mainline-core"
    "phase-11-launch-files-advanced|my_cpp_pkg|0|mainline-core"
    "phase-12-testing|my_cpp_pkg|1|mainline-core"
    "phase-13-actions-advanced|my_cpp_pkg|0|mainline-core"

    # --- mainline-tracks(Part 4 形體 + Track A/B 主線章節,有些需要 GPU/GUI)---
    "phase-15-urdf|my_robot_description|0|mainline-tracks"
    "phase-16-tf2|my_cpp_pkg|0|mainline-tracks"
    "phase-17-gazebo|my_gazebo_demo|0|mainline-tracks"
    "phase-18-ros2-control|my_robot_bringup|0|mainline-tracks"
    "phase-19-pluginlib|brake_strategy_base|0|mainline-tracks"
    "phase-19-pluginlib|brake_strategy_plugins|0|mainline-tracks"
    "phase-19-pluginlib|plugin_demo|0|mainline-tracks"
    "phase-20A-odometry-ekf|my_cpp_pkg|0|mainline-tracks"
    "phase-20B-arm-urdf|my_arm_description|0|mainline-tracks"
    "phase-21A-slam-toolbox|my_slam_demo|0|mainline-tracks"
    "phase-22A-nav2-basics|my_nav2_demo|0|mainline-tracks"
    "phase-22B-moveit-cpp|my_arm_moveit_config|0|mainline-tracks"
    "phase-22B-moveit-cpp|my_arm_moveit_demo|0|mainline-tracks"
    "phase-23A-nav2-bt-plugin|my_bt_plugin|1|mainline-tracks"
    "phase-26-dds-qos|my_cpp_pkg|0|mainline-tracks"

    # --- capstones ---
    "phase-14-capstone-1|my_cpp_pkg|1|capstones"
    "phase-CapstoneA-mobile|capstone_a|0|capstones"
    # phase-Capstone-Final 是 docker-only,colcon 跑不了,跳過

    # --- advanced(進階生態 5 章 — 跟 verify_advanced_phases.sh 一致)---
    "phase-30-nav2-bt-advanced|my_bt_advanced|1|advanced"
    "phase-32-rosbag2-advanced|my_bag_demo|0|advanced"
    "phase-35-foxglove-bridge|my_foxglove_demo|0|advanced"
    "phase-36-diagnostics-watchdog|my_diag_demo|1|advanced"
    "phase-37-lifecycle-diagnostics|my_lifecycle_diag|1|advanced"

    # --- advanced-drafts(advanced/ 文字草稿,預期會踩雷)---
    # 路徑格式特殊:advanced/<track>/<chapter>/code/<pkg>
    "advanced/multi-robot/01-namespace-spawn|multi_robot_demo|0|advanced-drafts"
    "advanced/perception/01-camera-cv-bridge|my_camera_demo|0|advanced-drafts"
    "advanced/perception/04-pcl-pointcloud|my_pcl_demo|0|advanced-drafts"
)

# === 解析參數 ===
GROUP="mainline-core"
KEEP_BUILD=0

for arg in "$@"; do
    case "${arg}" in
        --keep-build) KEEP_BUILD=1 ;;
        mainline-core|mainline-tracks|capstones|advanced|advanced-drafts|all)
            GROUP="${arg}"
            ;;
        *)
            echo "Unknown arg: ${arg}" >&2
            echo "Usage: $0 [mainline-core|mainline-tracks|capstones|advanced|advanced-drafts|all] [--keep-build]" >&2
            exit 2
            ;;
    esac
done

mkdir -p "${LOG_DIR}"

# === Helpers ===
log_section() {
    echo ""
    echo "======================================================"
    echo "  $1"
    echo "======================================================"
}

# === 開始 ===
log_section "verify_phases.sh — group=${GROUP} ts=${TS}"
echo "REPO_ROOT: ${REPO_ROOT}"
echo "WS_SRC:    ${WS_SRC}"
echo "LOG_DIR:   ${LOG_DIR}"
echo "GROUP:     ${GROUP}"

# 1. source ROS
if [[ -z "${ROS_DISTRO:-}" ]]; then
    if [[ -f /opt/ros/humble/setup.bash ]]; then
        source /opt/ros/humble/setup.bash
        echo "[ok] sourced /opt/ros/humble/setup.bash"
    else
        echo "[fatal] no /opt/ros/humble/setup.bash" >&2
        exit 2
    fi
fi

mkdir -p "${WS_SRC}"

# 2. 過濾要跑的 phases
declare -a SELECTED
for entry in "${PHASES[@]}"; do
    IFS='|' read -r phase_dir pkg_name has_gtest grp <<< "${entry}"
    if [[ "${GROUP}" == "all" || "${grp}" == "${GROUP}" ]]; then
        SELECTED+=("${entry}")
    fi
done

if [[ ${#SELECTED[@]} -eq 0 ]]; then
    echo "[warn] no phases selected for group=${GROUP}" >&2
    exit 0
fi

echo "[info] ${#SELECTED[@]} packages selected for group=${GROUP}"

# 3. clean build (除非 --keep-build)
# 注意:這支腳本「每章獨立 deploy + build」,因為主線多章 package name
# 同樣叫 my_cpp_pkg(刻意設計獨立可學),不能一次部署全部會互相覆蓋。
# verify_advanced_phases.sh 不用這樣是因為進階 5 章每章 pkg 名都不同。
if [[ ${KEEP_BUILD} -eq 0 ]]; then
    log_section "Step 1: clean build/install/log"
    cd "${HOME}/ros2_ws"
    rm -rf build install log
fi

# 4. 跑每章:每輪 deploy → build → (test) → 清 src(避免 my_cpp_pkg 互覆蓋)
declare -a RESULTS
log_section "Step 2: per-package deploy + build + test"
cd "${HOME}/ros2_ws"

# 用 unique key 紀錄每個 (phase_dir, pkg) 組合,避免 log 互覆蓋
declare -i idx=0
for entry in "${SELECTED[@]}"; do
    IFS='|' read -r phase_dir pkg_name has_gtest grp <<< "${entry}"
    idx+=1
    # log key 用 phase 短碼避免衝突(例:phase-08-custom-interfaces 內兩個 pkg)
    short=$(echo "${phase_dir}" | sed -E 's|^(phase-|advanced/[a-z-]+/)||;s|/.*||;s|-[a-z-]+$||')
    log_key="${idx}_${short}_${pkg_name}"

    src="${REPO_ROOT}/${phase_dir}/code/${pkg_name}"
    dst="${WS_SRC}/${pkg_name}"

    if [[ ! -d "${src}" ]]; then
        echo "[skip] ${log_key}: source not found at ${src}"
        RESULTS+=("${pkg_name}|${grp}|skipped|skipped|n/a")
        continue
    fi

    log_section "[${idx}/${#SELECTED[@]}] ${log_key}"
    pkg_log="${LOG_DIR}/${log_key}.log"

    # Deploy(覆蓋舊的)
    rm -rf "${dst}"
    cp -r "${src}" "${dst}"
    echo "[deploy] ${src} → ${dst}" | tee "${pkg_log}"

    # Build
    build_rc=0
    colcon build --packages-select "${pkg_name}" 2>&1 | tee -a "${pkg_log}"
    build_rc=${PIPESTATUS[0]}
    build_status="failed"
    [[ ${build_rc} -eq 0 ]] && build_status="passed"

    # Test(若有 gtest 且 build 過)
    test_status="skipped"
    if [[ "${has_gtest}" == "1" && ${build_rc} -eq 0 ]]; then
        log_section "Testing ${pkg_name}"
        test_rc=0
        colcon test --packages-select "${pkg_name}" 2>&1 | tee -a "${pkg_log}"
        colcon test-result --test-result-base "build/${pkg_name}" --verbose 2>&1 | tee -a "${pkg_log}"
        test_rc=${PIPESTATUS[0]}
        test_status="failed"
        [[ ${test_rc} -eq 0 ]] && test_status="passed"
    fi

    RESULTS+=("${pkg_name}|${grp}|${build_status}|${test_status}|${pkg_log}")
done

# 6. 寫 summary
log_section "Step 4: write summary"
{
    echo "# Verify Phases Log — group=${GROUP} ts=${TS}"
    echo ""
    echo "Run by: \`scripts/verify_phases.sh ${GROUP}\`"
    echo ""
    echo "REPO_ROOT: \`${REPO_ROOT}\`"
    echo ""
    echo "| Package | Group | Build | Test | Log |"
    echo "|---------|-------|-------|------|-----|"
    for r in "${RESULTS[@]}"; do
        IFS='|' read -r pkg grp b t lg <<< "${r}"
        case "${b}" in
            passed) b_emo="✅" ;;
            failed) b_emo="❌" ;;
            *) b_emo="⏸" ;;
        esac
        case "${t}" in
            passed) t_emo="✅" ;;
            failed) t_emo="❌" ;;
            skipped) t_emo="—" ;;
            *) t_emo="⏸" ;;
        esac
        lg_link="\`${lg}\`"
        echo "| ${pkg} | ${grp} | ${b_emo} ${b} | ${t_emo} ${t} | ${lg_link} |"
    done
    echo ""
    echo "**詳細 log 在 \`${LOG_DIR}/\`** — 每章一檔。"
    echo ""
    echo "## 注意"
    echo ""
    echo "- 這支腳本只測 colcon build / test,**不測 launch 跑得起來、機器人真的會動等 runtime 行為**"
    echo "- mainline-tracks / advanced-drafts 內某些章節需要 GPU(SLAM/Nav2)或實機 sensor,build 過 ≠ runtime demo 過"
    echo "- advanced/ 的 \`-drafts\` group 預期會有失敗,跑起來可以**順便發現需要修哪些雷**"
    echo "- 進階生態 5 章建議改用 \`verify_advanced_phases.sh\`(那支有完整 verify_log.md 紀錄)"
} > "${SUMMARY}"

cat "${SUMMARY}"
echo ""
echo "[done] summary written to ${SUMMARY}"

# 統計過 / 失敗數,讓 CI 可以用 exit code 判斷
n_fail=0
for r in "${RESULTS[@]}"; do
    IFS='|' read -r pkg grp b t lg <<< "${r}"
    [[ "${b}" == "failed" || "${t}" == "failed" ]] && ((n_fail++))
done

if [[ ${n_fail} -gt 0 ]]; then
    echo "[fail] ${n_fail} package(s) failed"
    exit 1
fi
echo "[pass] all packages OK"
