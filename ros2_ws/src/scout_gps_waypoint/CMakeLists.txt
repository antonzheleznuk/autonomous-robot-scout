cmake_minimum_required(VERSION 3.8)
project(scout_gps_waypoint)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Стандарт C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Поиск зависимостей
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_action REQUIRED)
find_package(nav2_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(std_msgs REQUIRED)
find_package(tf2_ros REQUIRED)
find_package(tf2_geometry_msgs REQUIRED)

include_directories(include)

# --- Исполняемый файл 1: gps_waypoint_node ---
add_executable(gps_waypoint_node
  src/gps_waypoint_node.cpp
)
ament_target_dependencies(gps_waypoint_node
  rclcpp
  geometry_msgs
  sensor_msgs
  nav_msgs
  tf2_ros
  tf2_geometry_msgs
)

# --- Исполняемый файл 2: mission_manager ---
add_executable(mission_manager
  src/mission_manager.cpp
)
ament_target_dependencies(mission_manager
  rclcpp
  rclcpp_action
  nav2_msgs
  geometry_msgs
  std_msgs
)

# Установка исполняемых файлов
install(TARGETS
  gps_waypoint_node
  mission_manager
  DESTINATION lib/${PROJECT_NAME}
)

# Установка конфигов, launch-файлов и include
install(DIRECTORY config launch include/
  DESTINATION share/${PROJECT_NAME}
)

ament_package()
