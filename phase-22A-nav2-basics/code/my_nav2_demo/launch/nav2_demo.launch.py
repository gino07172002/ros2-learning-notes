# nav2_demo.launch.py — Phase 22A
#
# 啟動 Nav2 完整 stack:
#   1. Phase 17 Gazebo + turtlebot3
#   2. nav2_bringup 的 bringup_launch.py(map_server + amcl + controller + planner + bt_navigator + ...)
#
# 使用者啟動後可:
#   - rviz 用 2D Pose Estimate 設初始位姿
#   - rviz 用 Nav2 Goal 點目標,看車自己規劃路徑跑過去
#   - CLI: ros2 action send_goal /navigate_to_pose nav2_msgs/action/NavigateToPose ...

import os
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription, TimerAction
from launch.launch_description_sources import PythonLaunchDescriptionSource
from ament_index_python.packages import get_package_share_directory


def generate_launch_description():
    pkg_share = get_package_share_directory('my_nav2_demo')
    nav2_params = os.path.join(pkg_share, 'config', 'nav2_params.yaml')
    map_yaml = os.path.join(pkg_share, 'maps', 'empty_4x4.yaml')

    nav2_bringup_pkg = get_package_share_directory('nav2_bringup')

    # 1. Phase 17 Gazebo + tb3
    gz_demo = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(get_package_share_directory('my_gazebo_demo'),
                         'launch', 'headless_demo.launch.py')),
    )

    # 2. Nav2 等 10s 後啟動(讓 gazebo 跟 robot_state_publisher 先穩定)
    nav2 = TimerAction(
        period=10.0,
        actions=[
            IncludeLaunchDescription(
                PythonLaunchDescriptionSource(
                    os.path.join(nav2_bringup_pkg, 'launch', 'bringup_launch.py')),
                launch_arguments={
                    'use_sim_time': 'true',
                    'map': map_yaml,
                    'params_file': nav2_params,
                    'autostart': 'true',
                }.items(),
            ),
        ]
    )

    return LaunchDescription([gz_demo, nav2])
