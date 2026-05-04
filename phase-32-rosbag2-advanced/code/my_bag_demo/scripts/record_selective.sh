#!/usr/bin/env bash
# record_selective.sh — Phase 32
#
# 白名單錄製 + MCAP backend + size/duration split。
# Usage:
#   bash record_selective.sh [output_dir]   (default: my_slam_bag)

set -euo pipefail

OUT_DIR="${1:-my_slam_bag}"

# SLAM 需要的最小 topic 集 — 不錄 camera / pointcloud(空間殺手)
TOPICS=(
  /scan
  /tf
  /tf_static
  /odom
  /clock
  /imu
)

# 500MB 切一檔,5 分鐘自動 split
ros2 bag record \
    "${TOPICS[@]}" \
    -o "${OUT_DIR}" \
    --storage mcap \
    --max-bag-size 500000000 \
    --max-bag-duration 300 \
    --compression-mode file \
    --compression-format zstd
