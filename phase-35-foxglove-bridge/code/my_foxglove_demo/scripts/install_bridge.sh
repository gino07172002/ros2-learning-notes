#!/usr/bin/env bash
# install_bridge.sh — Phase 35
#
# 一行裝 foxglove_bridge(WSL2 / Ubuntu 22.04 + ROS 2 Humble)。

set -euo pipefail

if dpkg -l | grep -q ros-humble-foxglove-bridge; then
    echo "[ok] foxglove_bridge already installed."
    exit 0
fi

sudo apt update
sudo apt install -y ros-humble-foxglove-bridge

echo ""
echo "[done] Now run:"
echo "  ros2 launch my_foxglove_demo bridge_only.launch.py"
echo "Then open https://app.foxglove.dev and connect to ws://localhost:8765"
