# 04_with_args.launch.py
# Launch arguments：讓使用者從 CLI 傳參數進來
#
# 用法：ros2 launch phase10_pkg 04_with_args.launch.py obstacle_distance:=0.3
#
# 應用場景：
#   - 同一份 launch 給「dev / prod / 測試」用，靠 args 切換
#   - 條件啟動某些 Node（debug=true 才開 rqt_graph）

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, ExecuteProcess
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    # 1. 宣告 launch arg + 預設值 + 說明
    obstacle_arg = DeclareLaunchArgument(
        'obstacle_distance',
        default_value='0.5',
        description='Distance (m) of fake obstacle in front of robot',
    )

    # 2. 取出 arg 值（這是 LaunchConfiguration 物件，不是 str）
    distance = LaunchConfiguration('obstacle_distance')

    return LaunchDescription([
        obstacle_arg,

        Node(
            package='turtlesim',
            executable='turtlesim_node',
            name='turtlesim',
        ),

        Node(
            package='phase08_pkg',
            executable='smart_brake_v2',
            name='smart_brake_v2',
            output='screen',
            remappings=[('cmd_vel', '/turtle1/cmd_vel')],
        ),

        # ExecuteProcess: 跑非 ROS 標準的命令（這裡是 fake_lidar.py 腳本）
        # cmd 內可以混用字串與 LaunchConfiguration（會被 substitute 成實際值）
        ExecuteProcess(
            cmd=['python3', '/home/gino/fake_lidar.py', distance],
            output='screen',
        ),
    ])
