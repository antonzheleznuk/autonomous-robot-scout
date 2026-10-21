"""
gazebo.launch.py — Запуск Gazebo Harmonic с роботом Scout Mini
в мире obstacle_course (препятствия + зоны старта/финиша)

Запускает:
1. Gazebo СЕРВЕР (-s): физика + сенсоры ogre2 (из world-файла) +
   программный рендер llvmpipe (LIBGL_ALWAYS_SOFTWARE=1)
2. Gazebo GUI (-g): отдельный процесс-клиент, аппаратный ogre
3. Спавн робота из URDF/Xacro в стартовой позиции A (-7,-7, 45°)
4. ros_gz_bridge: полный список топиков + GROUND TRUTH (эталонная
   поза из физики для измерения качества EKF/SLAM в RViz)
5. Статическая связка фреймов world → map (поза спавна), чтобы
   RViz показывал истину и оценки в одном дереве из корня world

Позиция старта переопределяется аргументами:
  ros2 launch scout_gazebo gazebo.launch.py start_x:=0 start_y:=0 start_yaw:=0
"""

import os
from launch import LaunchDescription
from launch.actions import (
    DeclareLaunchArgument,
    ExecuteProcess,
    IncludeLaunchDescription,
    SetEnvironmentVariable,
)
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import (
    Command,
    LaunchConfiguration,
    PathJoinSubstitution,
)
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare
from launch_ros.parameter_descriptions import ParameterValue


def generate_launch_description():
    """Генерирует описание запуска."""

    # --- Пути к пакетам ---
    pkg_scout_description = FindPackageShare('scout_description')
    pkg_scout_gazebo = FindPackageShare('scout_gazebo')

    # --- Путь к URDF/Xacro модели ---
    xacro_file = PathJoinSubstitution([
        pkg_scout_description, 'urdf', 'scout.urdf.xacro'
    ])

    # --- Генерация URDF из Xacro ---
    # ParameterValue(..., value_type=str) — обязательно для ROS 2 Jazzy
    robot_description = ParameterValue(
        Command(['xacro ', xacro_file]),
        value_type=str
    )

    # --- Путь к миру Gazebo (obstacle_course: препятствия + зоны) ---
    world_file = PathJoinSubstitution([
        pkg_scout_gazebo, 'worlds', 'obstacle_course.sdf'
    ])

    # --- Аргументы запуска ---
    use_sim_time_arg = DeclareLaunchArgument(
        'use_sim_time',
        default_value='true',
        description='Использовать время симуляции'
    )

    # --- Начальная позиция робота (старт в точке A) ---
    start_x = LaunchConfiguration('start_x')
    start_y = LaunchConfiguration('start_y')
    start_yaw = LaunchConfiguration('start_yaw')
    # 0.785 рад ≈ 45° — робот повёрнут в сторону финиша (7,7)

    start_x_arg = DeclareLaunchArgument(
        'start_x', default_value='-7.0',
        description='Начальная позиция X (точка A)')
    start_y_arg = DeclareLaunchArgument(
        'start_y', default_value='-7.0',
        description='Начальная позиция Y (точка A)')
    start_yaw_arg = DeclareLaunchArgument(
        'start_yaw', default_value='0.785',
        description='Начальный угол поворота (рад)')

    # --- Узел: Robot State Publisher ---
    robot_state_publisher = Node(
        package='robot_state_publisher',
        executable='robot_state_publisher',
        name='robot_state_publisher',
        output='screen',
        parameters=[{
            'robot_description': robot_description,
            'use_sim_time': LaunchConfiguration('use_sim_time'),
        }],
    )

    # --- Gazebo: СЕРВЕР (физика + сенсоры) ---
    # -s = только сервер. Сенсоры рендерятся ogre2 (из world-файла).
    # LIBGL_ALWAYS_SOFTWARE=1 ТОЛЬКО для сервера: llvmpipe даёт
    # корректный depth (аппаратный depth SVGA3D ненадёжен — проверено).
    gazebo_server = ExecuteProcess(
        cmd=['gz', 'sim', '-s', '-r', '-v', '4', world_file],
        additional_env={'LIBGL_ALWAYS_SOFTWARE': '1'},
        output='screen',
    )

    # --- Gazebo: GUI (окно симулятора, АППАРАТНЫЙ рендер) ---
    # -g = только GUI-клиент. Движок ogre — стабильный на VMware
    # SVGA3D (ogre2 в GUI мерцает).
    gazebo_gui = IncludeLaunchDescription(
        PythonLaunchDescriptionSource([
            PathJoinSubstitution([
                FindPackageShare('ros_gz_sim'),
                'launch', 'gz_sim.launch.py'
            ])
        ]),
        launch_arguments={
            'gz_args': ['-g -v 4 --render-engine ogre'],
        }.items(),
    )

    # --- Спавн робота в стартовой позиции ---
    spawn_robot = Node(
        package='ros_gz_sim',
        executable='create',
        name='spawn_scout',
        output='screen',
        arguments=[
            '-name', 'scout_mini',
            '-topic', 'robot_description',
            '-x', start_x,
            '-y', start_y,
            '-z', '0.1',
            '-R', '0.0',
            '-P', '0.0',
            '-Y', start_yaw,
        ],
    )

    # --- Связка фреймов: world → map (= поза спавна робота) ---
    # Вешаем на map (корень дерева оценок), НЕ на odom: у odom уже
    # есть родитель map→odom от SLAM — второй родитель расщепляет
    # TF-дерево на несвязанные части (ошибка "two unconnected trees").
    world_to_map_tf = Node(
        package='tf2_ros',
        executable='static_transform_publisher',
        name='world_to_map_tf',
        output='screen',
        arguments=[
            '--x', start_x,
            '--y', start_y,
            '--yaw', start_yaw,
            '--frame-id', 'world',
            '--child-frame-id', 'map',
        ],
    )







    # --- ros_gz_bridge (полный мост для всех сенсоров + ground truth) ---
    bridge = Node(
        package='ros_gz_bridge',
        executable='parameter_bridge',
        name='ros_gz_bridge',
        output='screen',
        arguments=[
            # Часы
            '/clock@rosgraph_msgs/msg/Clock[gz.msgs.Clock',
            # Привод
            '/cmd_vel@geometry_msgs/msg/Twist]gz.msgs.Twist',
            # Одометрия
            '/odom@nav_msgs/msg/Odometry[gz.msgs.Odometry',
            # Сочленения
            '/joint_states@sensor_msgs/msg/JointState[gz.msgs.Model',
            # LiDAR
            '/scan@sensor_msgs/msg/LaserScan[gz.msgs.LaserScan',
            # IMU
            '/imu@sensor_msgs/msg/Imu[gz.msgs.IMU',
            # GPS
            '/navsat@sensor_msgs/msg/NavSatFix[gz.msgs.NavSat',
            # Depth Camera
            '/depth_camera/image@sensor_msgs/msg/Image[gz.msgs.Image',
            '/depth_camera/depth_image@sensor_msgs/msg/Image[gz.msgs.Image',
            '/depth_camera/points@sensor_msgs/msg/PointCloud2[gz.msgs.PointCloudPacked',
            '/depth_camera/camera_info@sensor_msgs/msg/CameraInfo[gz.msgs.CameraInfo',
            # === GROUND TRUTH (эталонная поза из физики) ===
            '/model/scout_mini/odometry@nav_msgs/msg/Odometry[gz.msgs.Odometry',
            '/model/scout_mini/pose@tf2_msgs/msg/TFMessage[gz.msgs.Pose_V',
        ],
        parameters=[{
            'use_sim_time': LaunchConfiguration('use_sim_time'),
        }],
        remappings=[
            ('/depth_camera/image', '/depth_camera/image_raw'),
            ('/depth_camera/depth_image', '/depth_camera/depth/image_raw'),
            # Ground truth: удобное имя + TF в общее дерево
            ('/model/scout_mini/odometry', '/ground_truth/odometry'),
            ('/model/scout_mini/pose', '/tf'),
        ],
    )

    return LaunchDescription([
        use_sim_time_arg,
        start_x_arg,
        start_y_arg,
        start_yaw_arg,
        gazebo_server,
        gazebo_gui,
        robot_state_publisher,
        spawn_robot,
        world_to_map_tf,
        bridge,
    ])
