# control.launch.py
# 完整啟動 ros2_control 系統：
#   1. robot_state_publisher: 發 robot_description + TF
#   2. controller_manager: 載入 controllers.yaml + URDF 內的 ros2_control 區塊
#   3. spawner: 把 joint_state_broadcaster + velocity_controller 載入並 activate

import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import RegisterEventHandler, ExecuteProcess
from launch.event_handlers import OnProcessStart
from launch_ros.actions import Node
import xacro


def generate_launch_description():
    pkg_share = get_package_share_directory('my_robot_bringup')
    xacro_path = os.path.join(pkg_share, 'urdf', 'robot_with_control.urdf.xacro')
    controllers_yaml = os.path.join(pkg_share, 'config', 'controllers.yaml')

    robot_description = xacro.process_file(xacro_path).toxml()

    # ros2_control 系統的核心 Node
    controller_manager = Node(
        package='controller_manager',
        executable='ros2_control_node',
        parameters=[
            {'robot_description': robot_description},
            controllers_yaml,
        ],
        output='screen',
    )

    # robot_state_publisher
    rsp = Node(
        package='robot_state_publisher',
        executable='robot_state_publisher',
        parameters=[{'robot_description': robot_description}],
    )

    # spawner = 用 service call 通知 controller_manager 載入 + activate controller
    # 必須在 controller_manager 起來後才能呼叫，所以用 event handler 確保順序
    load_jsb = ExecuteProcess(
        cmd=['ros2', 'run', 'controller_manager', 'spawner', 'joint_state_broadcaster'],
        output='screen',
    )

    load_vel = ExecuteProcess(
        cmd=['ros2', 'run', 'controller_manager', 'spawner', 'velocity_controller'],
        output='screen',
    )

    return LaunchDescription([
        controller_manager,
        rsp,

        # controller_manager 起來後再 spawner（不然 service 還沒準備好）
        RegisterEventHandler(
            OnProcessStart(
                target_action=controller_manager,
                on_start=[load_jsb, load_vel],
            )
        ),
    ])
