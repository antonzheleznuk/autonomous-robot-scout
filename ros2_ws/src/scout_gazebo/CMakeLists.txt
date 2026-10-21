cmake_minimum_required(VERSION 3.8)
project(scout_gazebo)

find_package(ament_cmake REQUIRED)

# Установка миров, моделей и конфигураций
install(DIRECTORY worlds models launch config
  DESTINATION share/${PROJECT_NAME}
)

ament_package()
