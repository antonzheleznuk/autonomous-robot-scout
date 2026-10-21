#!/bin/bash
# ============================================================
# record.sh — ручная запись rosbag (когда rosbag-контейнер
# отключён флагом --no-rosbag, или для отдельной сессии)
# Использование: ./scripts/record.sh [длительность_сек]
# По умолчанию 60 секунд; завершение — по таймауту.
# ============================================================

cd "$(dirname "$0")/.."

DURATION=${1:-60}
BAG_NAME="scout_manual_$(date +%Y%m%d_%H%M%S)"

echo "=========================================="
echo "  Rosbag — ручная запись"
echo "  Длительность: ${DURATION} с"
echo "  Файл: bags/${BAG_NAME}"
echo "=========================================="

mkdir -p bags

docker run --rm --network host \
    -e ROS_DOMAIN_ID=0 \
    -e RMW_IMPLEMENTATION=rmw_cyclonedds_cpp \
    -v $(pwd)/bags:/bags \
    -v /dev/shm:/dev/shm \
    scout:rosbag \
    bash -c "
      source /opt/ros/jazzy/setup.bash &&
      timeout ${DURATION} ros2 bag record -o /bags/${BAG_NAME} --storage mcap --compression-mode file --compression-format zstd --topics /scan /imu /navsat /odom /odometry/filtered /cmd_vel /joint_states /robot_description /map /tf /tf_static /plan /received_global_plan /goal_pose /mission/status /global_costmap/costmap /local_costmap/costmap
    " || true

echo ""
echo "Запись завершена: bags/${BAG_NAME}"
ls -lh bags/${BAG_NAME}/
