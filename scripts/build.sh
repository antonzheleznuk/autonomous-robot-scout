#!/bin/bash
# ============================================================
# build.sh — сборка base + 7 сервисных Docker-образов
# (первый раз ~20-25 минут)
# Использование: ./scripts/build.sh
# ============================================================

set -e
cd "$(dirname "$0")/.."

echo "=========================================="
echo "  Scout Mini — Сборка Docker-образов"
echo "=========================================="
echo ""

if ! command -v docker &> /dev/null; then
    echo "ОШИБКА: Docker не установлен!"
    exit 1
fi

echo "Сборка базового образа scout:base (с версией из VERSION)..."
echo ""

docker build --build-arg SCOUT_VERSION="$(cat VERSION 2>/dev/null || echo dev)" \
    -t scout:base -f docker/Dockerfile.base .

echo ""
echo "Сборка 7 сервисных образов поверх base..."
echo ""

docker compose build --parallel

echo ""
echo "=========================================="
echo "  Сборка завершена. Образы:"
docker images | grep scout | awk '{print "    " $1 ":" $2 " (" $7 ")"}'
echo "=========================================="
