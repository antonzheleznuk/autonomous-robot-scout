/**
 * @file gps_waypoint_node.hpp
 * @brief Нода конвертации GPS-координат в map frame для навигации
 *
 * Читает waypoints из параметров (lat/lon), выполняет проекцию
 * Equirectangular ENU относительно датума мира и переносит
 * результат в map-фрейм через статическую TF world->map.
 *
 * Система координат проекта (реестр п.5, п.34):
 *  - ENU-датум = центр мира Gazebo (55.755826, 37.617300);
 *  - map-фрейм начинается в точке спавна A(мир -7,-7) и повёрнут
 *    на 45° (ось X = диагональ A->B), поэтому ENU-смещения
 *    ОБЯЗАТЕЛЬНО проходят трансформацию world->map.
 *
 * Автор: Scout Mini Team
 * Дата: 2025
 */

#ifndef GPS_WAYPOINT_NODE_HPP_
#define GPS_WAYPOINT_NODE_HPP_

#include <rclcpp/rclcpp.hpp>
#include <geometry_msgs/msg/pose_stamped.hpp>
#include <nav_msgs/msg/odometry.hpp>
#include <sensor_msgs/msg/nav_sat_fix.hpp>
#include <tf2_ros/transform_listener.h>
#include <tf2_ros/buffer.h>
#include <tf2_geometry_msgs/tf2_geometry_msgs.hpp>

#include <string>
#include <vector>
#include <cmath>

namespace scout_gps
{

/**
 * @struct GpsWaypoint
 * @brief Структура для хранения одной GPS-точки маршрута
 */
struct GpsWaypoint
{
  std::string name;       // Название точки (например, "Старт (A)")
  double latitude;        // Широта (градусы, WGS84)
  double longitude;       // Долгота (градусы, WGS84)
  double altitude;        // Высота (метры над уровнем моря)
  double yaw;             // Ориентация при прибытии (радианы, map-фрейм)
  double tolerance;       // Допуск прибытия (метры)
};

/**
 * @class GpsWaypointNode
 * @brief ROS 2 нода для конвертации GPS waypoints в map frame
 *
 * Алгоритм работы:
 * 1. Загружает waypoints из параметров (waypoints.yaml)
 * 2. Ждёт доступности статической TF world->map (публикуется
 *    контейнером gz_sim с момента старта системы)
 * 3. Проецирует каждую GPS-точку в ENU относительно датума
 *    (= координаты в мире Gazebo) и переносит в map через TF
 * 4. Публикует PoseStamped в map frame для RViz и mission_manager
 */
class GpsWaypointNode : public rclcpp::Node
{
public:
  /**
   * @brief Конструктор ноды
   * Инициализирует подписки, публикации и загружает параметры
   */
  explicit GpsWaypointNode(const rclcpp::NodeOptions & options = rclcpp::NodeOptions());

  /**
   * @brief Деструктор
   */
  ~GpsWaypointNode() override = default;

  /**
   * @brief Получить список сконвертированных waypoints в map frame
   * @return Вектор PoseStamped в map frame
   */
  std::vector<geometry_msgs::msg::PoseStamped> getWaypoints() const;

  /**
   * @brief Проверить, завершена ли конвертация GPS → map
   * @return true, если все waypoints сконвертированы
   */
  bool isReady() const;

private:
  // --- Callback-функции ---

  /**
   * @brief Обработчик GPS-одометрии от NavSat Transform
   * Диагностика: показывает позицию робота по GPS (map frame)
   * и подтверждает работоспособность navsat_transform_node
   */
  void gpsOdomCallback(const nav_msgs::msg::Odometry::SharedPtr msg);

  /**
   * @brief Обработчик сырых GPS-данных (NavSatFix)
   * Диагностика: контрольная сверка первого фикса с датумом
   * (ожидаем ENU-смещение старта ≈ (-7, -7) м от центра мира)
   */
  void navsatCallback(const sensor_msgs::msg::NavSatFix::SharedPtr msg);

  // --- Вспомогательные методы ---

  /**
   * @brief Загрузить waypoints из параметров ROS
   * Читает плоские ключи waypoints.i.* (см. waypoints.yaml)
   */
  void loadWaypoints();

  /**
   * @brief Конвертировать GPS-координаты в ENU (x, y) от датума
   * Equirectangular projection — точна для расстояний < 10 км.
   *
   * @param lat Широта точки (градусы)
   * @param lon Долгота точки (градусы)
   * @param ref_lat Широта датума (градусы)
   * @param ref_lon Долгота датума (градусы)
   * @param[out] x Локальная координата X (метры, Восток)
   * @param[out] y Локальная координата Y (метры, Север)
   */
  void gpsToLocal(
    double lat, double lon,
    double ref_lat, double ref_lon,
    double & x, double & y) const;

  /**
   * @brief Выполнить конвертацию всех waypoints
   * Вызывается из таймера, когда TF world->map доступна
   */
  void convertWaypoints();

  /**
   * @brief Опубликовать сконвертированные waypoints
   * (и при необходимости повторить попытку конвертации)
   */
  void publishWaypoints();

  // --- Константы ---
  static constexpr double EARTH_RADIUS = 6378137.0;  // Радиус Земли, м (WGS84)
  static constexpr double DEG_TO_RAD = M_PI / 180.0; // Перевод градусов в радианы

  // --- Переменные ---

  // Список GPS-точек маршрута (загружается из YAML)
  std::vector<GpsWaypoint> waypoints_;

  // Сконвертированные точки в map frame (для Nav2)
  std::vector<geometry_msgs::msg::PoseStamped> converted_waypoints_;

  // Датум ENU-проекции (центр мира Gazebo, из параметров)
  double datum_latitude_{55.755826};
  double datum_longitude_{37.617300};

  // Флаги диагностики/готовности
  bool first_fix_logged_{false};  // Первый /navsat уже сверен с датумом
  bool is_ready_{false};          // Все waypoints сконвертированы

  // --- ROS 2 интерфейсы ---

  // Подписка на GPS-одометрию от NavSat Transform (диагностика)
  rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr gps_odom_sub_;

  // Подписка на сырые GPS-данные (диагностическая сверка)
  rclcpp::Subscription<sensor_msgs::msg::NavSatFix>::SharedPtr navsat_sub_;

  // Публикация текущей цели (для визуализации в RViz)
  rclcpp::Publisher<geometry_msgs::msg::PoseStamped>::SharedPtr waypoint_pub_;

  // TF2 буфер и listener: перенос ENU(мир) -> map
  std::shared_ptr<tf2_ros::Buffer> tf_buffer_;
  std::shared_ptr<tf2_ros::TransformListener> tf_listener_;

  // Таймер: попытка конвертации + периодическая публикация (1 Гц)
  rclcpp::TimerBase::SharedPtr timer_;
};

}  // namespace scout_gps

#endif  // GPS_WAYPOINT_NODE_HPP_
