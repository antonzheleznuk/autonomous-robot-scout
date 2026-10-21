"""
gps_mission.launch.py — Запуск миссии по GPS-точкам

Запускает:
1. gps_waypoint_node (конвертация GPS координат)
2. mission_manager (конечный автомат управления автономным проездом)
"""

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    pkg_scout_gps = FindPackageShare('scout_gps_waypoint')

    use_sim_time_arg = DeclareLaunchArgument(
        'use_sim_time', default_value='true',
        description='Использовать время симуляции'
    )

    waypoints_file = PathJoinSubstitution([
        pkg_scout_gps, 'config', 'waypoints.yaml'
    ])

    gps_node = Node(
        package='scout_gps_waypoint',
        executable='gps_waypoint_node',
        name='gps_waypoint_node',
        output='screen',
        parameters=[
            waypoints_file,
            {'use_sim_time': LaunchConfiguration('use_sim_time')}
        ]
    )

    mission_node = Node(
        package='scout_gps_waypoint',
        executable='mission_manager',
        name='mission_manager',
        output='screen',
        parameters=[
            waypoints_file,
            {'use_sim_time': LaunchConfiguration('use_sim_time')}
        ]
    )

    return LaunchDescription([
        use_sim_time_arg,
        gps_node,
        mission_node
    ])
