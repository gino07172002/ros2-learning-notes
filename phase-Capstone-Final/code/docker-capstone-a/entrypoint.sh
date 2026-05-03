#!/bin/bash
# entrypoint.sh — Capstone Final
#
# source ROS + Capstone A workspace
# 顯示環境 + 把 PID 1 交給真正的 process(SIGTERM 才能正確接收)
set -e

source /opt/ros/humble/setup.bash
source /ws/install/setup.bash

echo "[capstone-final] ROS_DOMAIN_ID=${ROS_DOMAIN_ID:-0}"
echo "[capstone-final] TURTLEBOT3_MODEL=${TURTLEBOT3_MODEL:-burger}"
echo "[capstone-final] exec: $*"

exec "$@"
