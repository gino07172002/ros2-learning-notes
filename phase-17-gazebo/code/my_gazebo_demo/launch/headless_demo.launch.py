# headless_demo.launch.py — Phase 17
#
# 目標:在 WSL 啟動 Gazebo Classic(headless,no GUI)+ spawn TurtleBot3 burger
# 讓 SLAM/Nav2 後續章節有 simulator 可吃。
#
# 全部用 launch 啟動:
#   1. gzserver(物理引擎,沒 GUI)
#   2. robot_state_publisher 發 TurtleBot3 URDF + TF
#   3. spawn_entity.py 把 turtlebot3 放進 world
#
# 沒啟動 gzclient(GUI)。要看畫面需要 GPU 或 WSLg,請使用者本機自行加。

import os
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription, ExecuteProcess
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import Command, LaunchConfiguration
from launch_ros.actions import Node
from launch_ros.parameter_descriptions import ParameterValue
from ament_index_python.packages import get_package_share_directory


def generate_launch_description():
    pkg_share = get_package_share_directory('my_gazebo_demo')
    world_file = os.path.join(pkg_share, 'worlds', 'empty_with_walls.world')

    # ⚠️ 雷:turtlebot3_description/urdf/turtlebot3_burger.urdf 沒有 <gazebo> 標籤
    # 所以 spawn 後沒 /scan /odom。要用 turtlebot3_gazebo/models/turtlebot3_burger/model.sdf
    # 這個 SDF 帶完整的 ros plugin(differential_drive、laser、imu)
    tb3_sdf = os.path.join(
        get_package_share_directory('turtlebot3_gazebo'),
        'models', 'turtlebot3_burger', 'model.sdf')

    tb3_urdf_for_rsp = os.path.join(
        get_package_share_directory('turtlebot3_description'),
        'urdf', 'turtlebot3_burger.urdf')

    # Gazebo headless:用 gazebo.launch.py(自動帶 ROS plugins:factory/init/state)
    # 設 gui:=false 不啟動 gzclient(WSL 沒 GUI 也可)
    # ⚠️ 雷:用 gzserver.launch.py 反而會缺 gazebo_ros_factory,/spawn_entity 不存在
    gazebo_pkg = get_package_share_directory('gazebo_ros')
    gazebo = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(gazebo_pkg, 'launch', 'gazebo.launch.py')),
        launch_arguments={
            'world': world_file,
            'verbose': 'true',
            'gui': 'false',
        }.items(),
    )

    # robot_state_publisher 發 URDF(讓 SLAM/TF 知道 base_link 結構)
    # 這個 URDF 雖然有 xacro tag 但用了 default value,直接 read() 也行
    with open(tb3_urdf_for_rsp, 'r') as f:
        urdf_xml = f.read()

    rsp = Node(
        package='robot_state_publisher',
        executable='robot_state_publisher',
        parameters=[{'robot_description': urdf_xml,
                     'use_sim_time': True}],
        output='screen',
    )

    # spawn entity 到 Gazebo:用 -file 讀 SDF,這份 SDF 有完整 ros gazebo plugin
    spawn = Node(
        package='gazebo_ros',
        executable='spawn_entity.py',
        arguments=[
            '-entity', 'burger',
            '-file', tb3_sdf,
            '-x', '0.0', '-y', '0.0', '-z', '0.01',
        ],
        output='screen',
    )

    return LaunchDescription([gazebo, rsp, spawn])
