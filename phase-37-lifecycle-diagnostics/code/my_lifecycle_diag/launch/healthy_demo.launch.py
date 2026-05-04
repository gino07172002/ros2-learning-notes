# healthy_demo.launch.py — Phase 37
#
# 啟動:
#   1. healthy_lifecycle_node (unconfigured)
#   2. 自動 transition: configure (after 2s) → activate (after 4s)
#   3. diagnostic_aggregator (聚合 /diagnostics)
#
# 跑起來看 /diagnostics 跟 /diagnostics_agg:
#   t=0: unconfigured (WARN)
#   t=2: configured/inactive (WARN「閒置中」)
#   t=4: active (OK + tick_count 增長)
#
# 配 Phase 35 Foxglove 看樹狀變化最有感:
#   ros2 launch foxglove_bridge foxglove_bridge_launch.xml &
#   ros2 launch my_lifecycle_diag healthy_demo.launch.py

import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import ExecuteProcess, TimerAction
from launch_ros.actions import Node, LifecycleNode
from launch_ros.events.lifecycle import ChangeState
from launch.actions import EmitEvent
from launch_ros.event_handlers import OnStateTransition
from launch.actions import RegisterEventHandler
from lifecycle_msgs.msg import Transition


def generate_launch_description():
    diag_share = ''
    try:
        diag_share = get_package_share_directory('my_diag_demo')
    except Exception:
        pass  # Phase 36 沒裝就 fallback 到不開 aggregator

    node = LifecycleNode(
        package='my_lifecycle_diag',
        executable='healthy_lifecycle_node',
        name='healthy_lifecycle_node',
        namespace='',
        output='screen',
    )

    # 用 ExecuteProcess 直接 ros2 lifecycle set 是最穩的(避開 EmitEvent 雷)
    do_configure = TimerAction(
        period=2.0,
        actions=[ExecuteProcess(
            cmd=['ros2', 'lifecycle', 'set', '/healthy_lifecycle_node', 'configure'],
            output='screen',
        )]
    )
    do_activate = TimerAction(
        period=4.0,
        actions=[ExecuteProcess(
            cmd=['ros2', 'lifecycle', 'set', '/healthy_lifecycle_node', 'activate'],
            output='screen',
        )]
    )

    actions = [node, do_configure, do_activate]

    if diag_share:
        agg_yaml = os.path.join(diag_share, 'config', 'aggregator.yaml')
        if os.path.exists(agg_yaml):
            aggregator = Node(
                package='diagnostic_aggregator',
                executable='aggregator_node',
                name='diagnostic_aggregator',
                output='screen',
                parameters=[agg_yaml],
            )
            actions.append(aggregator)

    return LaunchDescription(actions)
