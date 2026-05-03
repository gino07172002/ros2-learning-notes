# 04_groups_namespace.launch.py
# GroupAction + PushRosNamespace：把多個 Node 包進同一個 namespace
#
# 場景：多機器人系統
#   - robot1/turtlesim、robot1/smart_brake
#   - robot2/turtlesim、robot2/smart_brake
#   兩台車 topic 完全不會混到（/robot1/cmd_vel vs /robot2/cmd_vel）
#
# 業界實際應用：機器人車隊、模擬多 agent 場景。

from launch import LaunchDescription
from launch.actions import GroupAction
from launch_ros.actions import Node, PushRosNamespace


def generate_launch_description():
    return LaunchDescription([
        # ── Robot 1 ──
        GroupAction([
            PushRosNamespace('robot1'),
            Node(package='turtlesim', executable='turtlesim_node', name='turtlesim'),
            Node(
                package='phase08_pkg',
                executable='smart_brake_v2',
                name='smart_brake_v2',
                output='screen',
                remappings=[('cmd_vel', 'turtle1/cmd_vel')],
                # 注意：在 namespace 內，相對 topic 'cmd_vel' 自動變 '/robot1/cmd_vel'
            ),
        ]),

        # ── Robot 2 ──
        GroupAction([
            PushRosNamespace('robot2'),
            Node(package='turtlesim', executable='turtlesim_node', name='turtlesim'),
            Node(
                package='phase08_pkg',
                executable='smart_brake_v2',
                name='smart_brake_v2',
                output='screen',
                remappings=[('cmd_vel', 'turtle1/cmd_vel')],
            ),
        ]),
    ])

# 啟動後 ros2 topic list 會看到：
#   /robot1/turtle1/cmd_vel
#   /robot1/turtle1/pose
#   /robot1/brake_status
#   /robot2/turtle1/cmd_vel
#   ...
