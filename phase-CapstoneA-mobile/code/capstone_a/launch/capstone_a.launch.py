# capstone_a.launch.py — 完整 Capstone A demo
#
# 啟動順序:
#   t=0    Phase 17 Gazebo + turtlebot3
#   t=10s  Nav2 stack(localization + navigation 兩組 lifecycle)
#   t=15s  自訂 BT plugin 已 load(Phase 23A,build 過就會被找到)
#   t=20s  auto_navigator 啟動
#   t=25s  發 /initialpose
#   t=45s  發第一個 goal(1.0, 0.0)
#   t=…    依 sequence 跑完三個 waypoint
#
# 跟個別 phase 的差別:
#   - Capstone 整合 SLAM/localization、Nav2、自動 navigator
#   - 等同實機部署的 launch 結構
#
# WSL 限制:GPU 不足,Nav2 各 lifecycle 會 active 但 controller 跑不順
# 雲端 / 實機跑可看到車真的循路徑跑完所有 waypoint

import os
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription, TimerAction
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory


def generate_launch_description():
    # 1. Nav2 stack(Phase 22A's nav2_demo 已包 Gazebo + Nav2)
    nav2 = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(get_package_share_directory('my_nav2_demo'),
                         'launch', 'nav2_demo.launch.py')),
    )

    # 2. 自動 navigator(30s 後啟動,讓 nav2 完全 active)
    # 依時序:t=0 gazebo / t=10 nav2 啟動 / t=15-20 lifecycle activate / t=30 navigator 開始送 goal
    auto_nav = TimerAction(
        period=30.0,
        actions=[
            Node(
                package='capstone_a',
                executable='auto_navigator',
                name='auto_navigator',
                output='screen',
                parameters=[{'use_sim_time': True}],
            ),
        ]
    )

    return LaunchDescription([nav2, auto_nav])
