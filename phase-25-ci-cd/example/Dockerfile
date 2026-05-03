# Dockerfile — 把整個 ROS workspace 包成可部署的 image
#
# 多階段 build：
#   - stage 1 (builder): 用 desktop image 編譯
#   - stage 2 (runtime): 用更小的 base image，只複製 install/

# === Stage 1: Builder ===
FROM osrf/ros:humble-desktop AS builder

WORKDIR /workspace
COPY . src/

# 安裝 deps + 編譯
RUN apt-get update && \
    rosdep update && \
    rosdep install --from-paths src --ignore-src -y --rosdistro humble && \
    rm -rf /var/lib/apt/lists/*

RUN bash -c "source /opt/ros/humble/setup.bash && \
             colcon build --merge-install"

# === Stage 2: Runtime ===
FROM osrf/ros:humble-ros-core

WORKDIR /workspace

# 只複製 build 結果
COPY --from=builder /workspace/install /workspace/install

# entrypoint 自動 source
COPY <<EOF /workspace/entrypoint.sh
#!/bin/bash
source /opt/ros/humble/setup.bash
source /workspace/install/setup.bash
exec "\$@"
EOF
RUN chmod +x /workspace/entrypoint.sh

ENTRYPOINT ["/workspace/entrypoint.sh"]
CMD ["bash"]
