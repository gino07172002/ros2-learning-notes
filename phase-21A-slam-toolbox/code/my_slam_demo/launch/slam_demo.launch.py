# slam_demo.launch.py — Phase 21A
#
# 啟動全套 SLAM demo,並自動驗證 + 自動推 cmd_vel:
#   1. Gazebo headless + turtlebot3
#   2. (delay 15s)slam_toolbox online_async
#   3. (delay 25s)持續發 /cmd_vel 讓車自轉建圖
#   4. (delay 30s+)定時打印 /map 跟 /tf 證據
#
# 全部 launch 內完成,不需要使用者手動推 cmd_vel

import os
from launch import LaunchDescription
from launch.actions import (
    IncludeLaunchDescription,
    TimerAction,
    ExecuteProcess,
)
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory


def generate_launch_description():
    pkg_share = get_package_share_directory('my_slam_demo')
    slam_yaml = os.path.join(pkg_share, 'config', 'slam_async.yaml')

    # 1. Gazebo + turtlebot3
    gz_demo = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(get_package_share_directory('my_gazebo_demo'),
                         'launch', 'headless_demo.launch.py')),
    )

    # 2. SLAM 等 Gazebo 起來(5s 後)
    # ⚠️ 雷:delay 太長(15s)會導致 SLAM 啟動時 scan 已積到 sim time 15s,
    # 但它 TF buffer 從 0s 才開始填 → 「scan timestamp earlier than cache」
    # 改 5s 讓 SLAM 跟 TF cache 早期就同步(WSL slam_toolbox 啟動本身要 ~3 秒)
    slam = TimerAction(
        period=5.0,
        actions=[
            Node(
                package='slam_toolbox',
                executable='async_slam_toolbox_node',
                name='slam_toolbox',
                output='screen',
                parameters=[slam_yaml],
            )
        ]
    )

    # 3. 自動推 cmd_vel(15s 後),讓車原地自轉,光達掃完一圈
    drive = TimerAction(
        period=15.0,
        actions=[
            ExecuteProcess(
                cmd=['ros2', 'topic', 'pub', '--rate', '10',
                     '/cmd_vel', 'geometry_msgs/Twist',
                     '{linear: {x: 0.05}, angular: {z: 0.5}}'],
                output='screen',
            )
        ]
    )

    return LaunchDescription([gz_demo, slam, drive])
