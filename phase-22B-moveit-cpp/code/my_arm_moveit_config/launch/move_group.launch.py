# move_group.launch.py — Phase 22B
#
# 啟動 MoveIt 完整 stack(headless,沒 RViz):
#   1. robot_state_publisher 發 URDF / TF
#   2. joint_state_publisher 發 /joint_states(模擬 joint state)
#   3. move_group node — MoveIt 核心,接 plan / execute requests
#
# 重點:用 moveit_configs_utils 一次組好 robot_description / kinematics / ompl 多個 yaml
# 這個套件是 MoveIt 2 提供,讓 launch 不用手寫一堆 parameters dict

from launch import LaunchDescription
from launch_ros.actions import Node
from launch.substitutions import Command
from launch_ros.parameter_descriptions import ParameterValue
from ament_index_python.packages import get_package_share_directory
import os
import yaml


def load_yaml(package_name, file_path):
    package_path = get_package_share_directory(package_name)
    absolute_file_path = os.path.join(package_path, file_path)
    with open(absolute_file_path, 'r') as f:
        return yaml.safe_load(f)


def generate_launch_description():
    arm_desc_pkg = get_package_share_directory('my_arm_description')
    moveit_pkg = get_package_share_directory('my_arm_moveit_config')

    xacro_file = os.path.join(arm_desc_pkg, 'urdf', 'arm.urdf.xacro')
    srdf_file = os.path.join(arm_desc_pkg, 'srdf', 'arm.srdf')

    # 展開 xacro 成 URDF string
    robot_description = {
        'robot_description': ParameterValue(
            Command(['xacro ', xacro_file]),
            value_type=str)
    }

    # SRDF 直接 read
    with open(srdf_file, 'r') as f:
        srdf_content = f.read()
    robot_description_semantic = {
        'robot_description_semantic': srdf_content
    }

    # 讀進其他 yaml
    kinematics = {'robot_description_kinematics':
                  load_yaml('my_arm_moveit_config', 'config/kinematics.yaml')}

    ompl_yaml = load_yaml('my_arm_moveit_config', 'config/ompl_planning.yaml')
    planning_pipeline = {
        'planning_pipelines': ['ompl'],
        'default_planning_pipeline': 'ompl',
        'ompl': ompl_yaml,
    }

    joint_limits = {'robot_description_planning':
                    load_yaml('my_arm_moveit_config', 'config/joint_limits.yaml')}

    controllers = load_yaml('my_arm_moveit_config', 'config/moveit_controllers.yaml')

    # 用 fake trajectory execution(沒接真 ros2_control,直接 echo 軌跡到 /joint_states)
    trajectory_execution = {
        'moveit_manage_controllers': False,
        'trajectory_execution.allowed_execution_duration_scaling': 1.2,
        'trajectory_execution.allowed_goal_duration_margin': 0.5,
        'trajectory_execution.allowed_start_tolerance': 0.01,
    }

    move_group = Node(
        package='moveit_ros_move_group',
        executable='move_group',
        output='screen',
        parameters=[
            robot_description,
            robot_description_semantic,
            kinematics,
            planning_pipeline,
            joint_limits,
            controllers,
            trajectory_execution,
            {'use_sim_time': False},
            # MoveIt 需要這個告訴它 fake controller 能直接執行
            {'moveit_simple_controller_manager.fake_arm_controller.default': True},
        ],
    )

    rsp = Node(
        package='robot_state_publisher',
        executable='robot_state_publisher',
        parameters=[robot_description],
        output='screen',
    )

    jsp = Node(
        package='joint_state_publisher',
        executable='joint_state_publisher',
        output='screen',
    )

    return LaunchDescription([rsp, jsp, move_group])
