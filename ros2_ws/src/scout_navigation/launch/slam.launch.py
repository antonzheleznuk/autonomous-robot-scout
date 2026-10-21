"""
slam.launch.py — Запуск SLAM Toolbox в режиме online_async

ВАЖНО (Jazzy): slam_toolbox — lifecycle-нода! Прямой запуск через Node(...)
оставляет её в состоянии unconfigured (тишина, нет /map). Запускаем через
LifecycleNode и сами выполняем переходы configure → activate — по образцу
официального online_async_launch.py (механизм autostart).
"""

from launch import LaunchDescription
from launch.actions import (
    DeclareLaunchArgument,
    EmitEvent,
    LogInfo,
    RegisterEventHandler,
)
from launch.conditions import IfCondition
from launch.events import matches_action
from launch.substitutions import (
    LaunchConfiguration,
    PathJoinSubstitution,
)
from launch_ros.actions import LifecycleNode
from launch_ros.event_handlers import OnStateTransition
from launch_ros.events.lifecycle import ChangeState
from launch_ros.substitutions import FindPackageShare
from lifecycle_msgs.msg import Transition


def generate_launch_description():
    """Генерирует описание запуска."""

    pkg_scout_navigation = FindPackageShare('scout_navigation')

    slam_params_file = PathJoinSubstitution([
        pkg_scout_navigation, 'config', 'slam_params.yaml'
    ])

    use_sim_time_arg = DeclareLaunchArgument(
        'use_sim_time', default_value='true',
        description='Использовать время симуляции')

    autostart_arg = DeclareLaunchArgument(
        'autostart', default_value='true',
        description='Автоматически выполнить configure + activate slam_toolbox')

    # --- SLAM Toolbox: LifecycleNode (не обычный Node!) ---
    slam_node = LifecycleNode(
        package='slam_toolbox',
        executable='async_slam_toolbox_node',
        name='slam_toolbox',
        output='screen',
        namespace='',
        parameters=[
            slam_params_file,
            {'use_sim_time': LaunchConfiguration('use_sim_time')},
        ],
    )

    # --- Автостарт, шаг 1: CONFIGURE сразу после запуска ---
    configure_event = EmitEvent(
        event=ChangeState(
            lifecycle_node_matcher=matches_action(slam_node),
            transition_id=Transition.TRANSITION_CONFIGURE,
        ),
        condition=IfCondition(LaunchConfiguration('autostart')),
    )

    # --- Автостарт, шаг 2: ACTIVATE, когда configure завершён ---
    activate_event = RegisterEventHandler(
        OnStateTransition(
            target_lifecycle_node=slam_node,
            start_state='configuring',
            goal_state='inactive',
            entities=[
                LogInfo(msg='[slam.launch] configure завершён, активируем slam_toolbox...'),
                EmitEvent(event=ChangeState(
                    lifecycle_node_matcher=matches_action(slam_node),
                    transition_id=Transition.TRANSITION_ACTIVATE,
                )),
            ],
        ),
        condition=IfCondition(LaunchConfiguration('autostart')),
    )

    return LaunchDescription([
        use_sim_time_arg,
        autostart_arg,
        slam_node,
        configure_event,
        activate_event,
    ])
