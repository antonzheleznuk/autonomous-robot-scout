#!/bin/bash
# ============================================================
# play_bag.sh — просмотр записи rosbag в RViz (v3)
#
# v3: после окончания записи RViz остаётся открытым —
#     закрытие вручную по Enter (или Ctrl+C); плеер получил
#     имя scout_bag_play (согласовано с stop_play.sh).
# v2: вспомогательный rsp-контейнер убран — /robot_description
#     и /joint_states пишутся в саму запись.
#
# Использование:
#   ./scripts/play_bag.sh                      — последняя запись, темп 1.0
#   ./scripts/play_bag.sh 2.0                  — последняя запись, темп 2.0
#   ./scripts/play_bag.sh scout_nav_ИМЯ 1.5    — конкретная запись
#
# Остановка воспроизведения: Ctrl+C. После конца записи RViz
# остаётся открытым (финальный кадр для изучения/скриншотов).
# Система (compose) должна быть остановлена.
# ============================================================
set -u
cd "$(dirname "$0")/.."

if [[ "${1:-}" =~ ^scout_nav ]]; then
  BAG_NAME="$1"; RATE="${2:-1.0}"
else
  RATE="${1:-1.0}"

  # Выбор по ИМЕНИ (таймстамп в имени), а не по mtime папки:
  # проигрывание пишет/удаляет кэш распаковки внутри папки и
  # меняет её mtime — ls -t начинал выбирать старую запись (п.43)
  BAG_NAME="$(ls bags | grep '^scout_nav' | sort -r | head -1)"

fi
BAG_DIR="bags/$BAG_NAME"

if [ ! -f "$BAG_DIR/metadata.yaml" ]; then
  echo "ОШИБКА: $BAG_DIR — не закрытая запись (нет metadata.yaml)."
  exit 1
fi
echo "=== Просмотр: $BAG_NAME, темп x$RATE ==="

xhost +local:docker 2>/dev/null || true

docker run -d --rm --name scout_rviz_play --network host \
    -e DISPLAY=$DISPLAY \
    -e QT_X11_NO_MITSHM=1 \
    -v /tmp/.X11-unix:/tmp/.X11-unix:rw \
    -v "$(pwd)/ros2_ws/src/scout_bringup/rviz/scout_nav.rviz:/ros2_ws/install/scout_bringup/share/scout_bringup/rviz/scout_nav.rviz" \
    scout:rviz \
    bash -c "
      source /opt/ros/jazzy/setup.bash &&
      rviz2 -d /ros2_ws/install/scout_bringup/share/scout_bringup/rviz/scout_nav.rviz --ros-args -p use_sim_time:=true
    "

cleanup() {
  echo ""
  echo "=== Остановка просмотра ==="
  docker kill scout_bag_play 2>/dev/null
  docker stop scout_rviz_play 2>/dev/null
  for z in "$BAG_DIR"/*.mcap.zstd; do
    rm -f "${z%.mcap.zstd}.mcap"
  done
}
trap cleanup EXIT

sleep 5   # дать RViz раскрыться
echo "=== Плеер запущен. Ctrl+C — остановить воспроизведение. ==="
docker run --rm --name scout_bag_play --network host \
    -v "$(pwd)/bags:/bags" \
    -e ROS_DOMAIN_ID=0 \
    -e RMW_IMPLEMENTATION=rmw_cyclonedds_cpp \
    scout:rosbag \
    bash -c "
      source /opt/ros/jazzy/setup.bash &&
      ros2 bag play /bags/$BAG_NAME --rate $RATE --clock
    "

# Запись доиграла (или остановлена Ctrl+C): RViz остаётся открыт
echo ""
echo "=== Запись воспроизведена. RViz остаётся открытым. ==="
echo "Нажмите Enter (или Ctrl+C), чтобы закрыть просмотр и удалить кэш распаковки..."
read -r
