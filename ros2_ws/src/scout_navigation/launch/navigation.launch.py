"""
navigation.launch.py — Запуск полного навигационного стека Nav2

Запускает все серверы Nav2:
- Planner Server (SmacPlanner2D)
- Controller Server (RegulatedPurePursuit)
- BT Navigator
- Behavior Server (spin/backup/wait)
- Velocity Smoother
- Collision Monitor
- Waypoint Follower
- Lifecycle Manager

НЕ запускает Gazebo, SLAM, EKF — они должны быть запущены отдельно!
"""

import os
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, GroupAction
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import Node, SetParameter
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    """Генерирует описание запуска."""

    pkg_scout_navigation = FindPackageShare('scout_navigation')

    use_sim_time_arg = DeclareLaunchArgument(
        'use_sim_time', default_value='true',
        description='Использовать время симуляции'
    )

    params_file = PathJoinSubstitution([
        pkg_scout_navigation, 'config', 'nav2_params.yaml'
    ])

    set_sim_time = SetParameter(
        name='use_sim_time',
        value=LaunchConfiguration('use_sim_time')
    )

    # --- Map Server (опционально, на будущее: загрузка сохранённой
    #     карты; сейчас карта приходит от SLAM через /map) ---
    map_server = Node(
        package='nav2_map_server',
        executable='map_server',
        name='map_server',
        output='screen',
        parameters=[{
            'use_sim_time': LaunchConfiguration('use_sim_time'),
            'yaml_filename': '',
        }],
    )

    # --- Lifecycle Manager (активирует все ноды; должен быть последним) ---
    lifecycle_manager = Node(
        package='nav2_lifecycle_manager',
        executable='lifecycle_manager',
        name='lifecycle_manager_navigation',
        output='screen',
        parameters=[
            params_file,
            {'use_sim_time': LaunchConfiguration('use_sim_time')},
        ],
    )

    planner_server = Node(
        package='nav2_planner',
        executable='planner_server',
        name='planner_server',
        output='screen',
        parameters=[
            params_file,
            {'use_sim_time': LaunchConfiguration('use_sim_time')},
        ],
        remappings=[
            ('/plan', '/plan'),
        ],
    )

    controller_server = Node(
        package='nav2_controller',
        executable='controller_server',
        name='controller_server',
        output='screen',
        parameters=[
            params_file,
            {'use_sim_time': LaunchConfiguration('use_sim_time')},
        ],
        remappings=[
            # Цепочка: контроллер НЕ пишет напрямую в /cmd_vel!
            ('cmd_vel', 'cmd_vel_nav'),
        ],
    )

    bt_navigator = Node(
        package='nav2_bt_navigator',
        executable='bt_navigator',
        name='bt_navigator',
        output='screen',
        parameters=[
            params_file,
            {'use_sim_time': LaunchConfiguration('use_sim_time')},
        ],
    )

    # ⚠️ Jazzy: пакет nav2_recoveries влит в nav2_behaviors (К5)
    behavior_server = Node(
        package='nav2_behaviors',
        executable='behavior_server',
        name='behavior_server',
        output='screen',
        parameters=[
            params_file,
            {'use_sim_time': LaunchConfiguration('use_sim_time')},
        ],
    )

    waypoint_follower = Node(
        package='nav2_waypoint_follower',
        executable='waypoint_follower',
        name='waypoint_follower',
        output='screen',
        parameters=[
            params_file,
            {'use_sim_time': LaunchConfiguration('use_sim_time')},
        ],
    )

    velocity_smoother = Node(
        package='nav2_velocity_smoother',
        executable='velocity_smoother',
        name='velocity_smoother',
        output='screen',
        parameters=[
            params_file,
            {'use_sim_time': LaunchConfiguration('use_sim_time')},
        ],
        remappings=[
            ('cmd_vel', 'cmd_vel_nav'),           # вход от контроллера
            ('cmd_vel_smoothed', 'cmd_vel_smoothed'),  # выход
        ],
    )

    collision_monitor = Node(
        package='nav2_collision_monitor',
        executable='collision_monitor',
        name='collision_monitor',
        output='screen',
        parameters=[
            params_file,
            {'use_sim_time': LaunchConfiguration('use_sim_time')},
        ],
        remappings=[
            ('cmd_vel_in', 'cmd_vel_smoothed'),
            ('cmd_vel_out', 'cmd_vel'),           # финал — к мосту Gazebo
        ],
    )

    return LaunchDescription([
        use_sim_time_arg,
        set_sim_time,

        planner_server,
        controller_server,
        behavior_server,

        bt_navigator,
        waypoint_follower,

        velocity_smoother,
        collision_monitor,

        lifecycle_manager,
    ])
