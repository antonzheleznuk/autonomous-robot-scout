#!/bin/bash
# ============================================================
# stop.sh — остановка системы Scout Mini (v2)
# Использование: ./scripts/stop.sh [--clean]
#   --clean — дополнительно удалить собранные образы и тома
#             (полный сброс до «git clone + build»)
# ============================================================

cd "$(dirname "$0")/.."

echo "=========================================="
echo "  Scout Mini — Остановка системы"
echo "=========================================="

echo "[1/2] Остановка контейнеров..."
docker compose down --remove-orphans 2>/dev/null

if [ "${1:-}" = "--clean" ]; then
    echo "[2/2] Полная очистка (образы и тома)..."
    docker compose down -v --rmi local --remove-orphans 2>/dev/null || true
else
    echo "[2/2] Пропуск очистки (--clean — для полного удаления)"
fi

xhost -local:docker 2>/dev/null || true

echo ""
echo "=== Проверка: ниже должно быть пусто ==="
docker compose ps
docker ps --filter name=scout_
