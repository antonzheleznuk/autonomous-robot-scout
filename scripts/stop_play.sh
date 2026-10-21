#!/bin/bash
# ============================================================
# stop_play.sh — остановить просмотр записи rosbag
# Останавливает плеер (kill — SIGTERM он игнорирует) и RViz,
# убирает кэш распаковки. Использовать из второго терминала,
# если Ctrl+C в окне плеера не сработал.
# ============================================================
cd "$(dirname "$0")/.."

# 1) Плеер
if docker ps --format '{{.Names}}' | grep -qx scout_bag_play; then
  docker kill scout_bag_play
  echo "Плеер остановлен."
else
  echo "Плеер не запущен."
fi

# 2) RViz просмотра (если запускался через play_bag.sh, он обычно
#    закроется сам после смерти плеера — это подстраховка)
docker stop scout_rviz_play 2>/dev/null && echo "RViz закрыт."

# 3) Кэш распаковки во всех записях
for z in bags/scout_nav_*/*.mcap.zstd; do
  rm -f "${z%.mcap.zstd}.mcap"
done
echo "Готово."
