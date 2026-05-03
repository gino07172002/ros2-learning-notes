# =============================================================================
# Phase 24 — Capstone 1 (ApproachController) Docker image
#
# Multi-stage build:
#   Stage 1 (builder)  — 裝 build deps、colcon build
#   Stage 2 (runtime)  — 只帶 install/、執行用 image 體積小
#
# Build context 是 repo root,讓我們能讀 sibling phase 的 code
# (從 phase-24-docker/code/docker-capstone/ 看,context = ../../../../)
#
# 用法見 docker-compose.yml,直接 docker build 也可:
#   cd d:/ros_learn/ros2-learning-notes
#   docker build -f phase-24-docker/code/docker-capstone/Dockerfile -t capstone1:latest .
# =============================================================================

# -----------------------------------------------------------------------------
# Stage 1: builder
# -----------------------------------------------------------------------------
# 用 ros:humble 完整 desktop-like base,包含 colcon、rosidl 工具
# 換成 ros:humble-ros-base 也可,但 rosidl_default_generators 要自己裝
FROM ros:humble AS builder

ARG DEBIAN_FRONTEND=noninteractive

# 裝 build 工具(ros:humble 已有 colcon,但 rosidl 生成 C++ 需要 ament_cmake)
RUN apt-get update && apt-get install -y --no-install-recommends \
        python3-colcon-common-extensions \
        python3-rosdep \
        ros-humble-rclcpp-lifecycle \
        ros-humble-rclcpp-action \
        ros-humble-lifecycle-msgs \
        ros-humble-sensor-msgs \
        ros-humble-geometry-msgs \
        build-essential \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /ws

# 把需要的兩個 phase 的 code 帶進來
# my_robot_interfaces 從 Phase 08(已含 Phase 13 擴充的 Countdown.action + Phase 14 用的 Approach.action)
# capstone code 從 Phase 14
COPY phase-08-custom-interfaces/code/my_robot_interfaces  src/my_robot_interfaces
COPY phase-14-capstone-1/code/my_cpp_pkg                  src/phase14_capstone

# 改名:套件叫 phase14_capstone
# launch file 內寫死 package='phase14_pkg'(因為使用者本機 WSL 部署時習慣叫這名)
# 在這裡一併改成 phase14_capstone
RUN sed -i 's|<name>my_cpp_pkg</name>|<name>phase14_capstone</name>|' src/phase14_capstone/package.xml \
 && sed -i 's|project(my_cpp_pkg)|project(phase14_capstone)|'        src/phase14_capstone/CMakeLists.txt \
 && sed -i "s|package='phase14_pkg'|package='phase14_capstone'|"     src/phase14_capstone/launch/capstone.launch.py

# colcon build(ros:humble 預設沒 source 環境,要先 source)
RUN /bin/bash -c "source /opt/ros/humble/setup.bash && \
                  colcon build --packages-select my_robot_interfaces phase14_capstone \
                               --cmake-args -DBUILD_TESTING=OFF"

# -----------------------------------------------------------------------------
# Stage 2: runtime
# -----------------------------------------------------------------------------
# 用較小的 ros-core base,不含 desktop tools(rviz、gazebo)
# 體積:ros:humble (~2GB) → ros:humble-ros-core (~600MB)
FROM ros:humble-ros-core

ARG DEBIAN_FRONTEND=noninteractive

# runtime 只需要這些 lib
RUN apt-get update && apt-get install -y --no-install-recommends \
        ros-humble-rclcpp-lifecycle \
        ros-humble-rclcpp-action \
        ros-humble-lifecycle-msgs \
        ros-humble-sensor-msgs \
        ros-humble-geometry-msgs \
        ros-humble-launch-ros \
        ros-humble-rclpy \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /ws

# 從 builder stage 把 install/ 抓過來(這就是 multi-stage 的精華)
COPY --from=builder /ws/install /ws/install

# entrypoint 自動 source 環境後 exec 給的指令
COPY phase-24-docker/code/docker-capstone/entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

# 預設啟動 capstone launch file
ENTRYPOINT ["/entrypoint.sh"]
CMD ["ros2", "launch", "phase14_capstone", "capstone.launch.py"]
