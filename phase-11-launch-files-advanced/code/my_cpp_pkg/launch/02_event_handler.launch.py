# 02_event_handler.launch.py
# Event Handler：根據其他 process 的事件觸發動作
#
# 場景：
#   - turtlesim 啟動成功後 → 才啟動 smart_brake_v2（避免太早 publish 訊息丟失）
#   - 任何 Node 結束（crash 或 Ctrl+C）→ 印警告或重啟
#   - 監控 launch 整體生命週期

from launch import LaunchDescription
from launch.actions import (
    LogInfo,
    RegisterEventHandler,
)
from launch.event_handlers import (
    OnProcessStart,
    OnProcessExit,
)
from launch_ros.actions import Node


def generate_launch_description():
    turtlesim = Node(
        package='turtlesim',
        executable='turtlesim_node',
        name='turtlesim',
    )

    smart_brake = Node(
        package='phase08_pkg',
        executable='smart_brake_v2',
        name='smart_brake_v2',
        output='screen',
        remappings=[('cmd_vel', '/turtle1/cmd_vel')],
    )

    return LaunchDescription([
        turtlesim,

        # 事件 1：turtlesim 啟動成功 → 印 log + 啟動 smart_brake
        RegisterEventHandler(
            OnProcessStart(
                target_action=turtlesim,
                on_start=[
                    LogInfo(msg='✅ turtlesim 起來了，啟動 smart_brake_v2'),
                    smart_brake,
                ]
            )
        ),

        # 事件 2：smart_brake 結束（crash 或被殺）→ 印警告
        RegisterEventHandler(
            OnProcessExit(
                target_action=smart_brake,
                on_exit=[
                    LogInfo(msg='❌ smart_brake_v2 已結束！整個系統可能進入不安全狀態'),
                ]
            )
        ),
    ])
