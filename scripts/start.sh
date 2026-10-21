#!/bin/bash
# ============================================================
# start.sh — запуск системы Scout Mini (v2)
#
# Использование:
#   ./scripts/start.sh                 — полная система
#   ./scripts/start.sh --no-rviz       — без RViz (headless)
#   ./scripts/start.sh --no-rosbag     — без записи rosbag
#   ./scripts/start.sh --no-mission    — без автостарта миссии
#                                          (ручные цели: ./scripts/mission.sh)
#
# Флаги можно комбинировать. Отключение — через
# docker compose --scale <сервис>=0 (compose v2).
# ============================================================

set -e
cd "$(dirname "$0")/.."

PROJECT_VERSION="$(cat VERSION 2>/dev/null || echo dev)"
export SCOUT_VERSION="$PROJECT_VERSION"   # проброс в docker compose 

SCALE_ARGS=()
for arg in "$@"; do
    case $arg in
        --no-rviz)    echo "  [--no-rviz]      RViz отключён";      SCALE_ARGS+=(--scale rviz=0) ;;
        --no-rosbag)  echo "  [--no-rosbag]    запись отключена";   SCALE_ARGS+=(--scale rosbag=0) ;;
        --no-mission) echo "  [--no-mission]   автостарт миссии отключён"; SCALE_ARGS+=(--scale gps_waypoint=0) ;;
        *)
            echo "Неизвестный аргумент: $arg"
            echo "Допустимо: --no-rviz | --no-rosbag | --no-mission"
            exit 1
            ;;
    esac
done

echo "=========================================="
echo "  Scout Mini — Автономная навигация  [v${PROJECT_VERSION}]"
echo "=========================================="

echo "[1/4] Проверка окружения..."
if [ -z "$DISPLAY" ]; then
    echo "  ОШИБКА: DISPLAY не установлен (нужна графическая сессия)."
    echo "  Запустите из графического терминала или: export DISPLAY=:0"
    exit 1
fi
echo "  [OK] DISPLAY=$DISPLAY"
if ! docker info &> /dev/null; then
    echo "  ОШИБКА: Docker не запущен или нет прав!"
    echo "  Попробуйте: sudo systemctl start docker"
    exit 1
fi
echo "  [OK] Docker запущен"

echo "[2/4] Настройка X11 для GUI-контейнеров..."
xhost +local:docker 2>/dev/null || true
echo "  [OK] X11 доступ разрешён"

echo "[3/4] Подготовка директорий..."
mkdir -p bags
echo "  [OK] bags/ готова"

echo "[4/4] Остановка старых и запуск системы..."
docker compose down 2>/dev/null || true

echo ""
echo "  Контейнеры:"
echo "    [1] Gazebo Harmonic      (симулятор + ground truth)"
echo "    [2] EKF + NavSat         (локализация)"
echo "    [3] SLAM Toolbox         (карта)"
echo "    [4] Nav2                 (Smac + RegulatedPurePursuit)"
echo "    [5] GPS-миссия A→B       (автономный проезд)"
echo "    [6] RViz2                (визуализация)"
echo "    [7] Rosbag               (запись 17 топиков)"
echo ""
echo "  До старта миссии: ~60-90 c (каскад healthcheck)"
echo "  Остановка: Ctrl+C, затем ./scripts/stop.sh"
echo "=========================================="

docker compose up --build "${SCALE_ARGS[@]}"
