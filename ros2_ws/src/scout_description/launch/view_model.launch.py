"""
view_model.launch.py — Визуализация модели робота в RViz

Запускает:
1. robot_state_publisher (публикует URDF и TF)
2. joint_state_publisher_gui (ползунки для сочленений)
3. RViz2 (3D-визуализация)

Использование:
  ros2 launch scout_description view_model.launch.py
"""

from launch import LaunchDescription
from launch.substitutions import Command, PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare
from launch_ros.parameter_descriptions import ParameterValue


def generate_launch_description():
    # Путь к Xacro-файлу модели
    pkg_scout_description = FindPackageShare('scout_description')
    xacro_file = PathJoinSubstitution([
        pkg_scout_description, 'urdf', 'scout.urdf.xacro'
    ])

    # Генерация URDF из Xacro
    # ParameterValue(..., value_type=str) — КЛЮЧЕВОЕ ИСПРАВЛЕНИЕ для Jazzy!
    # Говорит ROS 2: "это строка, не парси как YAML"
    robot_description = ParameterValue(
        Command(['xacro ', xacro_file]),
        value_type=str
    )

    # Robot State Publisher — публикует URDF и TF-дерево
    robot_state_publisher = Node(
        package='robot_state_publisher',
        executable='robot_state_publisher',
        name='robot_state_publisher',
        output='screen',
        parameters=[{
            'robot_description': robot_description,
            'use_sim_time': False,
        }],
    )

    # Joint State Publisher GUI — ползунки для вращения колёс
    joint_state_publisher = Node(
        package='joint_state_publisher_gui',
        executable='joint_state_publisher_gui',
        name='joint_state_publisher_gui',
    )

    # RViz2 — 3D-визуализация
    rviz = Node(
        package='rviz2',
        executable='rviz2',
        name='rviz2',
        output='screen',
    )

    return LaunchDescription([
        robot_state_publisher,
        joint_state_publisher,
        rviz,
    ])
