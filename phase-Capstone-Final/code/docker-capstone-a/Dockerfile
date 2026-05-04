# =============================================================================
# Capstone Final — Capstone A 完整 Docker 化
#
# 把 Track A 整套(Phase 17 Gazebo + 21A SLAM + 22A Nav2 + 23A BT + Capstone A)
# 包成一個 image。docker compose up 一鍵跑出 mobile robot 自主導航 demo。
#
# Multi-stage:
#   Stage 1 builder  — 完整 ros:humble desktop,colcon build 全部 phase
#   Stage 2 runtime  — ros:humble-ros-base + 必要 plugin,執行用 image
#
# 注意:Capstone A 本身依賴
#   my_gazebo_demo (Phase 17)
#   my_slam_demo  (Phase 21A,可選)
#   my_nav2_demo  (Phase 22A)
#   my_bt_plugin  (Phase 23A)
#   capstone_a    (本身)
#
# context = repo root,讓 Dockerfile 能 COPY 各 phase
# =============================================================================

# -----------------------------------------------------------------------------
# Stage 1: builder — 帶完整 ROS + Gazebo + Nav2 + slam_toolbox
# -----------------------------------------------------------------------------
FROM osrf/ros:humble-desktop AS builder

ARG DEBIAN_FRONTEND=noninteractive

# Capstone A 跑時需要的所有 ros 套件
RUN apt-get update && apt-get install -y --no-install-recommends \
        ros-humble-gazebo-ros-pkgs \
        ros-humble-turtlebot3-gazebo \
        ros-humble-turtlebot3-description \
        ros-humble-slam-toolbox \
        ros-humble-navigation2 \
        ros-humble-nav2-bringup \
        ros-humble-robot-localization \
        ros-humble-rmw-fastrtps-cpp \
        python3-colcon-common-extensions \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /ws

# 複製所有依賴 phase 的 code
# 注意:capstone_a/package.xml 內 <exec_depend>my_slam_demo</exec_depend>,
# 即使 capstone_a.launch.py 沒實際 include,colcon dependency check 仍要找到該 package
# 不帶它 → CI 在乾淨環境會 build 失敗(本機因 sibling phase 已 install 才能過)
COPY phase-17-gazebo/code/my_gazebo_demo                    src/my_gazebo_demo
COPY phase-21A-slam-toolbox/code/my_slam_demo               src/my_slam_demo
COPY phase-22A-nav2-basics/code/my_nav2_demo                src/my_nav2_demo
COPY phase-23A-nav2-bt-plugin/code/my_bt_plugin             src/my_bt_plugin
COPY phase-CapstoneA-mobile/code/capstone_a                 src/capstone_a

# colcon build 全部
# --event-handlers console_direct+ 讓 stderr 即時送到 stdout,CI 失敗時看得到原因
RUN /bin/bash -c "source /opt/ros/humble/setup.bash && \
                  colcon build --packages-select \
                    my_gazebo_demo my_slam_demo my_nav2_demo my_bt_plugin capstone_a \
                    --cmake-args -DBUILD_TESTING=OFF \
                    --event-handlers console_direct+"

# -----------------------------------------------------------------------------
# Stage 2: runtime — ros-base + Gazebo + nav2 + slam,排除 desktop tools
# -----------------------------------------------------------------------------
FROM osrf/ros:humble-desktop AS runtime
# 註:Capstone A 跑時要 gazebo headless,desktop image 帶 Gazebo
# 不切 ros-base 因為 turtlebot3_gazebo 還是要 gazebo binary

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
        ros-humble-gazebo-ros-pkgs \
        ros-humble-turtlebot3-gazebo \
        ros-humble-turtlebot3-description \
        ros-humble-slam-toolbox \
        ros-humble-navigation2 \
        ros-humble-nav2-bringup \
        ros-humble-rmw-fastrtps-cpp \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /ws
COPY --from=builder /ws/install /ws/install

# entrypoint 自動 source + 設 TURTLEBOT3_MODEL
COPY phase-Capstone-Final/code/docker-capstone-a/entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

ENV TURTLEBOT3_MODEL=burger

ENTRYPOINT ["/entrypoint.sh"]
CMD ["ros2", "launch", "capstone_a", "capstone_a.launch.py"]
