#!/bin/bash
# entrypoint.sh — container 啟動時自動 source ROS 環境後 exec 給的指令
#
# 為什麼要這樣寫:
#   ros:humble base image 預設不會 source /opt/ros/humble/setup.bash
#   每個 RUN/CMD 都是新的 shell,環境變數不會跨層保留
#   所以必須在這裡 source,然後 exec "$@" 把 PID 1 交給真正的 process
#   (用 exec 才能正確接 SIGTERM,docker stop 才會優雅關閉)

set -e

source /opt/ros/humble/setup.bash
source /ws/install/setup.bash

# 把 ROS_DOMAIN_ID 印出來,方便 debug 多 container 通訊
echo "[entrypoint] ROS_DOMAIN_ID=${ROS_DOMAIN_ID:-0}"
echo "[entrypoint] exec: $*"

exec "$@"
