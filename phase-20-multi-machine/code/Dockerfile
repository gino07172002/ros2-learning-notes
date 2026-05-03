# =============================================================================
# Phase 20 — multi-machine demo image
#
# 比 Phase 24 的 image 更輕量:不需要 capstone build 過的東西
# 直接用 ros:humble-ros-base + demo_nodes_cpp 當 talker/listener
#
# 為什麼自己建一個:
#   Phase 24 的 capstone1:latest 是 ros-core base(沒 demo_nodes_cpp)
#   多機通訊章只需要最簡單的 pub-sub + fastdds 工具,用 ros-base 即可
# =============================================================================

FROM ros:humble-ros-base

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
        ros-humble-demo-nodes-cpp \
        ros-humble-demo-nodes-py \
        ros-humble-rmw-fastrtps-cpp \
    && rm -rf /var/lib/apt/lists/*

# entrypoint:source 環境後 exec 真的指令
RUN echo '#!/bin/bash' > /entrypoint.sh \
 && echo 'set -e' >> /entrypoint.sh \
 && echo 'source /opt/ros/humble/setup.bash' >> /entrypoint.sh \
 && echo 'echo "[entrypoint] ROS_DOMAIN_ID=${ROS_DOMAIN_ID:-0}"' >> /entrypoint.sh \
 && echo 'echo "[entrypoint] ROS_DISCOVERY_SERVER=${ROS_DISCOVERY_SERVER:-<unset>}"' >> /entrypoint.sh \
 && echo 'echo "[entrypoint] exec: $*"' >> /entrypoint.sh \
 && echo 'exec "$@"' >> /entrypoint.sh \
 && chmod +x /entrypoint.sh

ENTRYPOINT ["/entrypoint.sh"]
CMD ["bash"]
