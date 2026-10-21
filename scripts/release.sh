#!/bin/bash
# ============================================================
# release.sh — релиз версии из файла VERSION (единственный
# источник истины): проверка чистоты репо, аннотированный тег
# vX.Y.Z и push. Release на GitHub создаст CI (release.yml).
# Использование: ./scripts/release.sh
# ============================================================
set -e
cd "$(dirname "$0")/.."

V="$(cat VERSION)"
grep -Eq '^[0-9]+\.[0-9]+\.[0-9]+$' <<< "$V" || { echo "VERSION не в формате X.Y.Z: '$V'"; exit 1; }
grep -q "^## \[$V\]" CHANGELOG.md \
  || { echo "В CHANGELOG.md нет секции [## $V] — допишите изменения перед релизом."; exit 1; }

if [ -n "$(git status --porcelain)" ]; then
  echo "Рабочее дерево грязное — закоммитьте изменения перед релизом."; exit 1
fi
if git rev-parse "v$V" >/dev/null 2>&1; then
  echo "Тег v$V уже существует локально."; exit 1
fi
if git ls-remote --exit-code --tags origin "v$V" >/dev/null 2>&1; then
  echo "Тег v$V уже существует на GitHub (удалите релиз и тег перед повтором):"
  echo "  git push origin :refs/tags/v$V && git tag -d v$V"
  exit 1
fi
git tag -a "v$V" -m "Release v$V"
git push origin "v$V"
echo "Релиз v$V: тег отправлен. GitHub Actions создаст Release автоматически."
