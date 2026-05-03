# capstone.launch.py
# 完整啟動：ApproachController (LifecycleNode) + 自動 configure+activate
#
# 用 event_handler 確保啟動順序：
#   Node 起來 → 自動送 configure → 自動送 activate
#
# 真正的業界 ROS launch 都這樣寫——不能讓使用者手動 lifecycle set

from launch import LaunchDescription
from launch.actions import (
    EmitEvent,
    LogInfo,
    RegisterEventHandler,
)
from launch.event_handlers import OnProcessStart
from launch_ros.actions import LifecycleNode
from launch_ros.events.lifecycle import ChangeState
import lifecycle_msgs.msg


def generate_launch_description():
    controller = LifecycleNode(
        package='phase14_pkg',
        executable='approach_controller',
        name='approach_controller',
        namespace='',
        output='screen',
    )

    # 啟動成功 → 送 configure
    configure_event = EmitEvent(event=ChangeState(
        lifecycle_node_matcher=lambda action: True,
        transition_id=lifecycle_msgs.msg.Transition.TRANSITION_CONFIGURE,
    ))

    # configure 成功（走捷徑：等 1 秒）→ 送 activate
    # 嚴謹做法是用 OnStateTransition event handler，但這樣對教學示範足夠
    from launch.actions import TimerAction

    activate_event = EmitEvent(event=ChangeState(
        lifecycle_node_matcher=lambda action: True,
        transition_id=lifecycle_msgs.msg.Transition.TRANSITION_ACTIVATE,
    ))

    return LaunchDescription([
        controller,

        # process 起來後立刻送 configure
        RegisterEventHandler(
            OnProcessStart(
                target_action=controller,
                on_start=[
                    LogInfo(msg='✅ controller process up, sending configure'),
                    configure_event,
                ]
            )
        ),

        # 1 秒後送 activate（簡化版——production 應監聽 state transition）
        TimerAction(
            period=1.5,
            actions=[
                LogInfo(msg='✅ sending activate'),
                activate_event,
            ]
        ),
    ])
