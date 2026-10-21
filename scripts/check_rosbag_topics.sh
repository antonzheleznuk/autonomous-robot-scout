#!/bin/bash
# ============================================================
# check_rosbag_topics.sh — контроль полноты записи rosbag (v3)
#
# v3: "последняя" запись выбирается по ИМЕНИ (таймстамп в имени:
#     scout_nav_ГГГГММДД_ЧЧММСС), а не по mtime папки —
#     проигрывание пишет/удаляет кэш распаковки внутри папки
#     и меняло её mtime, из-за чего ls -t выбирал старую запись
#     (реестр п.43). Добавлен необязательный аргумент имени
#     записи для режима bag.
# v2: /local_plan заменён на /received_global_plan (топик RPP,
#     реестр п.31); bag-режим проверяет, что запись ЗАКРЫТА.
#
# Режимы:
#   ./scripts/check_rosbag_topics.sh                  авто:
#                                                     контейнер жив -> live,
#                                                     иначе последняя запись
#   ./scripts/check_rosbag_topics.sh live             подписки работающего scout_rosbag
#   ./scripts/check_rosbag_topics.sh bag              последняя запись по имени
#   ./scripts/check_rosbag_topics.sh bag ИМЯ_ЗАПИСИ   конкретная запись
#                                                     (например scout_nav_20261019_174105
#                                                     или scout_manual_...)
#
# Запускать из корня проекта (рядом с папкой bags/).
# ROS 2 на хосте не нужен — всё выполняется в контейнерах.
# ============================================================

set -u

# --- Ожидаемые 17 топиков (1-в-1 с command сервиса rosbag) ---
EXPECTED=(
  /scan
  /imu
  /navsat
  /odom
  /odometry/filtered
  /cmd_vel
  /joint_states
  /robot_description
  /map
  /tf
  /tf_static
  /plan
  /received_global_plan
  /goal_pose
  /mission/status
  /global_costmap/costmap
  /local_costmap/costmap
)

MODE="${1:-auto}"
BAG_SEL="${2:-}"
BAGS_DIR="${BAGS_DIR:-$(pwd)/bags}"

container_running() {
  docker ps --format '{{.Names}}' | grep -qx scout_rosbag
}

# ------------------------------------------------------------
# LIVE: какие топики recorder уже успел подписать
# (строки "Subscribed to topic '/xxx'" в логах контейнера)
# ------------------------------------------------------------
check_live() {
  echo "=== LIVE: подписки контейнера scout_rosbag ==="
  local found
  found=$(docker logs scout_rosbag 2>&1 \
    | grep -o "Subscribed to topic '[^']*'" \
    | cut -d"'" -f2 | sort -u)

  local missing=0
  for t in "${EXPECTED[@]}"; do
    if printf '%s\n' "$found" | grep -qx "$t"; then
      echo "  [OK]   $t"
    else
      echo "  [НЕТ]  $t"
      missing=$((missing+1))
    fi
  done

  echo ""
  echo "Итого: $(( ${#EXPECTED[@]} - missing )) / ${#EXPECTED[@]}"
}

# ------------------------------------------------------------
# BAG: топики и число сообщений в последней ЗАКРЫТОЙ записи
# ------------------------------------------------------------
check_bag() {
  echo "=== BAG: анализ записи в $BAGS_DIR ==="

  # Выбор по ИМЕНИ (таймстамп в имени), не по mtime: просмотр
  # записи меняет mtime её папки — кэш распаковки (реестр п.43)
  local bag="$BAG_SEL"
  if [ -z "$bag" ]; then
    bag="$(ls "$BAGS_DIR" 2>/dev/null | grep '^scout_nav' | sort -r | head -1)"
  fi
  if [ -z "$bag" ]; then
    echo "Записи не найдены ($BAGS_DIR/scout_nav_*)."
    exit 1
  fi
  echo "Запись: $bag"
  echo ""

  # Запись открыта, пока rosbag работает: metadata.yaml дописывается
  # ТОЛЬКО при штатном закрытии (Ctrl+C -> docker compose down).
  if [ ! -f "$BAGS_DIR/$bag/metadata.yaml" ]; then
    echo ""
    echo "ОШИБКА: metadata.yaml отсутствует — запись ещё ОТКРЫТА"
    echo "или неполна. Завершите систему (Ctrl+C в терминале compose,"
    echo "затем docker compose down) и запустите проверку снова."
    exit 1
  fi

  # Ошибки не глотаем: если info упадёт — покажем почему
  local info
  info=$(docker run --rm -v "$BAGS_DIR:/bags:ro" scout:rosbag bash -c "
    source /opt/ros/jazzy/setup.bash &&
    ros2 bag info /bags/$bag")
  if [ -z "$info" ]; then
    echo "ОШИБКА: ros2 bag info не вернул вывод."
    exit 1
  fi

  echo "$info" | grep -E "Duration|^Messages|Bag size"
  echo ""

  # Парсинг строк вида:
  # Topic: /cmd_vel | Type: ... | Count: 123 | Serialization Format: cdr
  # Первая строка таблицы склеена с заголовком "Topic information:"
  # (реестр: правка awk из раздела 6)
  local parsed
  parsed=$(echo "$info" | grep "Topic:" \
    | awk -F'|' '{
        t=$1; sub(/^ *Topic information: */, "", t); gsub(/^ *Topic: *| *$/, "", t);
        c=$3; gsub(/^ *Count: *| *$/, "", c);
        print t" "c
      }' | sort)

  local missing=0 empty=0
  for t in "${EXPECTED[@]}"; do
    local line
    line=$(printf '%s\n' "$parsed" | grep "^$t ")
    if [ -n "$line" ]; then
      local cnt=${line#$t }
      if [ "$cnt" -gt 0 ]; then
        printf "  [OK]    %-28s %8s сообщ.\n" "$t" "$cnt"
      else
        printf "  [ПУСТО] %-28s %8s сообщ.\n" "$t" "$cnt"
        empty=$((empty+1))
      fi
    else
      printf "  [НЕТ]   %-28s топик отсутствует в записи\n" "$t"
      missing=$((missing+1))
    fi
  done

  echo ""
  echo "Итого: $(( ${#EXPECTED[@]} - missing )) / ${#EXPECTED[@]} топиков в записи."
  if [ "$empty" -gt 0 ]; then
    echo "Внимание: $empty топик(ов) записаны, но без сообщений."
    echo "(Например, /goal_pose пуст, если цель задавалась через"
    echo "action CLI, а не кликом RViz — это нормально.)"
  fi
}

case "$MODE" in
  live) check_live ;;
  bag)  check_bag ;;
  auto)
    if container_running; then check_live
    else check_bag; fi ;;
  *) echo "Неизвестный режим: $MODE (допустимо: live | bag [имя_записи])"; exit 1 ;;
esac
