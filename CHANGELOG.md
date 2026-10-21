# Changelog

Все значимые изменения проекта Scout Mini документируются в этом файле.

Формат основан на [Keep a Changelog](https://keepachangelog.com/ru/1.1.0/),
версионирование соответствует [Semantic Versioning](https://semver.org/).
Конвенция: буква `v` — только в git-тегах и GitHub-релизах (`vX.Y.Z`);
в файлах проекта — чистая версия (`X.Y.Z`).

## [Unreleased]
### Added
- (пусто — заполняется по ходу работы над следующей версией)

## [0.1.0] - 2026-10-21
### Added
- Первый этап проекта — полная система автономного робота:
  - 7 контейнеров Docker Compose с healthcheck-каскадом запуска
  - Симуляция: Gazebo Harmonic, арена 20x20 м, 7 препятствий,
    GPS-привязка (Москва), сенсоры: LiDAR 720@10 Гц, depth-камера,
    IMU 100 Гц, GPS 5 Гц
  - Локализация: EKF (robot_localization, колёса-скорости + IMU-yaw)
    + SLAM Toolbox (плотные коррекции map->odom)
  - Навигация: Nav2 (SmacPlanner2D + RegulatedPurePursuit),
    recovery (spin/backup/wait), collision monitor
  - Автономная GPS-миссия A->B на C++ нодах (gps_waypoint_node +
    mission_manager): конечный автомат, повторы, отложенный запрос
    результата, защита от preempt-гонки среды
  - Запись rosbag 17 топиков (MCAP+zstd, самодостаточная запись)
    и воспроизведение (play_bag.sh)
  - Система версионирования: VERSION (единый источник), CI-валидация,
    авто-релизы из тегов, OCI-метка версии в образах, CHANGELOG
  - Скрипты: start/stop/build/record/mission/play_bag/stop_play/
    check_rosbag_topics/release (+ флаги --no-rviz/--no-rosbag/--no-mission)
  - README (RU+EN), CHANGELOG, воспроизводимость: git clone -> build.sh -> start.sh
