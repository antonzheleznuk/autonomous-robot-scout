#!/bin/bash
# ============================================================
# Скрипт автоматической настройки хоста для проекта Scout Mini
# Запуск: chmod +x scripts/setup_host.sh && ./scripts/setup_host.sh
# ============================================================

set -e  # Останавливать скрипт при любой ошибке

echo "=========================================="
echo "  Настройка хоста для Scout Mini"
echo "=========================================="

# --- Проверка ОС ---
if [ "$(lsb_release -cs)" != "noble" ]; then
    echo "ОШИБКА: Скрипт предназначен для Ubuntu 24.04 (noble)"
    exit 1
fi

# --- Обновление системы ---
echo "[1/6] Обновление системы..."
sudo apt update && sudo apt full-upgrade -y

# --- Базовые утилиты ---
echo "[2/6] Установка базовых утилит..."
sudo apt install -y \
    curl wget gnupg2 lsb-release ca-certificates \
    apt-transport-https software-properties-common \
    build-essential cmake python3-pip python3-venv \
    htop net-tools iputils-ping nano vim tree \
    unzip p7zip-full jq git

# --- Docker ---
echo "[3/6] Установка Docker..."
sudo apt remove -y docker docker-engine docker.io containerd runc 2>/dev/null || true
sudo install -m 0755 -d /etc/apt/keyrings
curl -fsSL https://download.docker.com/linux/ubuntu/gpg | \
    sudo gpg --dearmor -o /etc/apt/keyrings/docker.gpg
echo "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.gpg] \
    https://download.docker.com/linux/ubuntu $(. /etc/os-release && echo "$VERSION_CODENAME") stable" | \
    sudo tee /etc/apt/sources.list.d/docker.list > /dev/null
sudo apt update
sudo apt install -y docker-ce docker-ce-cli containerd.io \
    docker-buildx-plugin docker-compose-plugin
sudo usermod -aG docker $USER

# --- Настройка логирования Docker ---
echo "[4/6] Настройка Docker..."
sudo tee /etc/docker/daemon.json > /dev/null <<EOF
{
    "log-driver": "json-file",
    "log-opts": { "max-size": "10m", "max-file": "3" },
    "storage-driver": "overlay2"
}
EOF
sudo systemctl enable docker
sudo systemctl restart docker

# --- Git ---
echo "[5/6] Настройка Git..."
read -p "Имя для Git (user.name): " git_name
read -p "Email для Git (user.email): " git_email
git config --global user.name "$git_name"
git config --global user.email "$git_email"
git config --global init.defaultBranch main
git config --global core.editor nano

# --- SSH-ключ ---
echo "[6/6] Генерация SSH-ключа..."
if [ ! -f ~/.ssh/id_ed25519 ]; then
    ssh-keygen -t ed25519 -C "$git_email" -N ""
    echo "Публичный ключ (добавьте на GitHub):"
    cat ~/.ssh/id_ed25519.pub
else
    echo "SSH-ключ уже существует."
fi

echo "=========================================="
echo "  Настройка завершена!"
echo "  Перезагрузите систему: sudo reboot"
echo "=========================================="
