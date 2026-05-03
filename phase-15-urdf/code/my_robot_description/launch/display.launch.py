# display.launch.py - 啟動 robot_state_publisher 廣播 TF
#
# 流程：
#   1. xacro 把 .urdf.xacro 展開成純 .urdf
#   2. 把 URDF 作為字串塞到 robot_state_publisher 的 robot_description 參數
#   3. robot_state_publisher 訂閱 /joint_states，發布 /tf 與 /tf_static
#
# 沒接 joint_state_publisher，所以 continuous joint（輪子）會固定 0 度
# Phase 16 TF2 章會教怎麼讓輪子轉動

import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch_ros.actions import Node
import xacro


def generate_launch_description():
    pkg_share = get_package_share_directory('my_robot_description')
    xacro_path = os.path.join(pkg_share, 'urdf', 'diffbot.urdf.xacro')

    # xacro 展開成 URDF 字串
    robot_description = xacro.process_file(xacro_path).toxml()

    return LaunchDescription([
        # robot_state_publisher: 把 URDF 內的 link 階層 → /tf
        Node(
            package='robot_state_publisher',
            executable='robot_state_publisher',
            name='robot_state_publisher',
            output='screen',
            parameters=[{'robot_description': robot_description}],
        ),

        # joint_state_publisher: 模擬發送 joint_states（不然 continuous joint 不會出現在 /tf）
        Node(
            package='joint_state_publisher',
            executable='joint_state_publisher',
            name='joint_state_publisher',
            output='screen',
        ),
    ])
