"""
localization.launch.py — Запуск локализации (EKF + NavSat Transform)

Запускает:
1. EKF-фильтр (odom + IMU → filtered odometry + TF odom→base_footprint)
2. NavSat Transform (GPS → /odometry/gps; TF не используется — глобальная привязка от SLAM)
"""

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    """Генерирует описание запуска."""

    pkg_scout_navigation = FindPackageShare('scout_navigation')

    # --- Аргументы ---
    use_sim_time_arg = DeclareLaunchArgument(
        'use_sim_time', default_value='true',
        description='Использовать время симуляции'
    )

    ekf_params_file = PathJoinSubstitution([
        pkg_scout_navigation, 'config', 'ekf_params.yaml'
    ])

    # --- EKF Node ---
    # Объединяет одометрию колёс и IMU
    ekf_node = Node(
        package='robot_localization',
        executable='ekf_node',
        name='ekf_filter_node',
        output='screen',
        parameters=[
            ekf_params_file,
            {'use_sim_time': LaunchConfiguration('use_sim_time')},
        ],
    )

    # --- NavSat Transform Node ---
    # Конвертирует GPS-координаты в локальные (map frame)
    navsat_node = Node(
        package='robot_localization',
        executable='navsat_transform_node',
        name='navsat_transform_node',
        output='screen',
        parameters=[
            ekf_params_file,
            {'use_sim_time': LaunchConfiguration('use_sim_time')},
        ],
        remappings=[
            # NavSat Transform ожидает стандартные имена топиков
            ('imu', '/imu'),
            ('gps/fix', '/navsat'),
            ('odometry/filtered', '/odometry/filtered'),
        ],
    )

    return LaunchDescription([
        use_sim_time_arg,
        ekf_node,
        navsat_node,
    ])
