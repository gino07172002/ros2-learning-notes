#!/usr/bin/env bash
# play_with_qos_override.sh — Phase 32
#
# 重播 bag,強制 sensor topic 用 BestEffort/Volatile,並發 /clock。
# Usage:
#   bash play_with_qos_override.sh <bag_dir> [rate]  (default rate: 1.0)

set -euo pipefail

BAG_DIR="${1:?Usage: $0 <bag_dir> [rate]}"
RATE="${2:-1.0}"

QOS_FILE="$(dirname "$0")/../config/qos_override.yaml"

ros2 bag play "${BAG_DIR}" \
    --clock 100 \
    --rate "${RATE}" \
    --qos-profile-overrides-path "${QOS_FILE}"
