#!/bin/bash
# ============================================================
# mission.sh — отправка цели Nav2 (координаты в фрейме map)
# Использование:
#   ./scripts/mission.sh            — финиш B (19.8, 0)
#   ./scripts/mission.sh 0 0        — возврат в A (0, 0)
#   ./scripts/mission.sh X Y        — произвольная точка
# Альтернатива: инструмент Nav2 Goal в RViz (клик по карте).
# ============================================================
set -e
X="${1:-19.8}"
Y="${2:-0.0}"

echo "=== Цель: ($X, $Y) в map ==="
docker run --rm -it --network host \
    -e ROS_DOMAIN_ID=0 \
    -e RMW_IMPLEMENTATION=rmw_cyclonedds_cpp \
    scout:navigation \
    bash -c "
      source /opt/ros/jazzy/setup.bash &&
      ros2 action send_goal /navigate_to_pose nav2_msgs/action/NavigateToPose '{
        pose: {
          header: {frame_id: \"map\"},
          pose: {
            position: {x: $X, y: $Y, z: 0.0},
            orientation: {x: 0.0, y: 0.0, z: 0.0, w: 1.0}
          }
        }
      }'
    "
