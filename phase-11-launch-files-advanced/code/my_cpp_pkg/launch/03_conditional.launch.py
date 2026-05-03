# 03_conditional.launch.py
# 條件啟動：用 IfCondition / UnlessCondition 決定某 Node 要不要啟
#
# 場景：
#   - launch arg debug:=true → 才啟動 rqt_graph + rqt_console
#   - launch arg sim:=true → 啟動 fake_lidar；sim:=false → 連真機光達
#   - 同一份 launch 給「dev / staging / prod」用

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.conditions import IfCondition, UnlessCondition
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    debug_arg = DeclareLaunchArgument(
        'debug',
        default_value='false',
        description='Set true to also launch rqt_graph for debug',
    )

    use_fake_lidar_arg = DeclareLaunchArgument(
        'use_fake_lidar',
        default_value='true',
        description='true=use fake_lidar.py, false=expect real lidar topic',
    )

    debug = LaunchConfiguration('debug')
    use_fake = LaunchConfiguration('use_fake_lidar')

    return LaunchDescription([
        debug_arg,
        use_fake_lidar_arg,

        # 一律啟動
        Node(package='turtlesim', executable='turtlesim_node', name='turtlesim'),
        Node(
            package='phase08_pkg',
            executable='smart_brake_v2',
            name='smart_brake_v2',
            output='screen',
            remappings=[('cmd_vel', '/turtle1/cmd_vel')],
        ),

        # 條件 1：debug=true 才啟 rqt_graph
        Node(
            package='rqt_graph',
            executable='rqt_graph',
            name='debug_graph',
            condition=IfCondition(debug),
        ),

        # 條件 2：use_fake_lidar=true 才跑 fake_lidar.py
        # 用 ExecuteProcess + LaunchConfiguration 結合（注意：cmd 是 list）
        # 真實情境會用 IncludeLaunchDescription 換上正版 lidar driver launch
    ])

# 用法：
#   ros2 launch phase11_pkg 03_conditional.launch.py
#   ros2 launch phase11_pkg 03_conditional.launch.py debug:=true
#   ros2 launch phase11_pkg 03_conditional.launch.py debug:=true use_fake_lidar:=false
