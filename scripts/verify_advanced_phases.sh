#!/usr/bin/env bash
# verify_advanced_phases.sh
#
# 一鍵驗證進階生態 5 章(Phase 30/32/35/36/37):
#   - 部署 code 到 ~/ros2_ws/src/
#   - colcon build --packages-select <each>
#   - colcon test (有 gtest 的章節)
#   - 收集成 verify_log.md(時間戳 + 每章狀態)
#
# 跑法:
#   bash scripts/verify_advanced_phases.sh
#   bash scripts/verify_advanced_phases.sh --keep-build  # 不清舊 build
#
# 預期輸出:
#   verify_log.md        — Markdown 表格摘要(每章 ✅/❌ + 詳細 log 連結)
#   /tmp/verify_<ts>/    — 每章獨立 log

set -o pipefail

# === 設定 ===
REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WS_SRC="${HOME}/ros2_ws/src"
TS="$(date +%Y%m%d_%H%M%S)"
LOG_DIR="/tmp/verify_${TS}"
SUMMARY="${REPO_ROOT}/verify_log.md"

KEEP_BUILD=0
[[ "${1:-}" == "--keep-build" ]] && KEEP_BUILD=1

mkdir -p "${LOG_DIR}"

# 章節定義: phase_dir | pkg_dirname | has_gtest
# pkg_dirname = code/ 下面的子資料夾名稱(也是 colcon 的 package name)
PHASES=(
    "phase-30-nav2-bt-advanced|my_bt_advanced|1"
    "phase-32-rosbag2-advanced|my_bag_demo|0"
    "phase-35-foxglove-bridge|my_foxglove_demo|0"
    "phase-36-diagnostics-watchdog|my_diag_demo|1"
    "phase-37-lifecycle-diagnostics|my_lifecycle_diag|1"
)

# === Helpers ===
log_section() {
    echo ""
    echo "======================================================"
    echo "  $1"
    echo "======================================================"
}

# === 開始 ===
log_section "verify_advanced_phases.sh — ${TS}"
echo "REPO_ROOT: ${REPO_ROOT}"
echo "WS_SRC:    ${WS_SRC}"
echo "LOG_DIR:   ${LOG_DIR}"

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

# 2. 部署
log_section "Step 1: deploy code"
for entry in "${PHASES[@]}"; do
    IFS='|' read -r phase_dir pkg_name has_gtest <<< "${entry}"
    src="${REPO_ROOT}/${phase_dir}/code/${pkg_name}"
    dst="${WS_SRC}/${pkg_name}"
    if [[ ! -d "${src}" ]]; then
        echo "[skip] ${pkg_name}: source not found at ${src}"
        continue
    fi
    rm -rf "${dst}"
    cp -r "${src}" "${dst}"
    echo "[deploy] ${pkg_name} → ${dst}"
done

# 3. clean build (除非 --keep-build)
if [[ ${KEEP_BUILD} -eq 0 ]]; then
    log_section "Step 2: clean build/install/log"
    cd "${HOME}/ros2_ws"
    rm -rf build install log
fi

# 4. 跑每章
declare -a RESULTS  # 形式: "pkg|build_status|test_status|log_file"
log_section "Step 3: build + test each phase"
cd "${HOME}/ros2_ws"
for entry in "${PHASES[@]}"; do
    IFS='|' read -r phase_dir pkg_name has_gtest <<< "${entry}"
    if [[ ! -d "${WS_SRC}/${pkg_name}" ]]; then
        RESULTS+=("${pkg_name}|skipped|skipped|n/a")
        continue
    fi
    log_section "Building ${pkg_name}"
    pkg_log="${LOG_DIR}/${pkg_name}.log"
    build_rc=0
    colcon build --packages-select "${pkg_name}" 2>&1 | tee "${pkg_log}"
    build_rc=${PIPESTATUS[0]}
    build_status="failed"
    [[ ${build_rc} -eq 0 ]] && build_status="passed"

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
    RESULTS+=("${pkg_name}|${build_status}|${test_status}|${pkg_log}")
done

# 5. 寫 summary
log_section "Step 4: write summary"
{
    echo "# Verify Log — ${TS}"
    echo ""
    echo "進階生態 5 章 colcon build + colcon test 自動驗證結果。"
    echo ""
    echo "Run by: \`scripts/verify_advanced_phases.sh\`"
    echo ""
    echo "| Package | Build | Test | Log |"
    echo "|---------|-------|------|-----|"
    for r in "${RESULTS[@]}"; do
        IFS='|' read -r pkg b t lg <<< "${r}"
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
        echo "| ${pkg} | ${b_emo} ${b} | ${t_emo} ${t} | ${lg_link} |"
    done
    echo ""
    echo "**詳細 log 在 \`${LOG_DIR}/\`** — 每章一檔。"
} > "${SUMMARY}"

cat "${SUMMARY}"
echo ""
echo "[done] summary written to ${SUMMARY}"
