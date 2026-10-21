/**
 * @file gps_waypoint_node.cpp
 * @brief Реализация ноды конвертации GPS → map frame
 *
 * Нода читает GPS-координаты из параметров, дожидается
 * статической TF world->map и конвертирует все waypoints
 * в координаты map frame (GPS → ENU(мир) → map).
 */

#include "gps_waypoint_node.hpp"

namespace scout_gps
{

GpsWaypointNode::GpsWaypointNode(const rclcpp::NodeOptions & options)
: Node("gps_waypoint_node", options)
{
  RCLCPP_INFO(this->get_logger(), "=== GPS Waypoint Node: инициализация ===");

  // --- Инициализация TF2 ---
  // Перенос ENU-координат (осей мира Gazebo) в map-фрейм
  tf_buffer_ = std::make_shared<tf2_ros::Buffer>(this->get_clock());
  tf_listener_ = std::make_shared<tf2_ros::TransformListener>(*tf_buffer_);

  // --- Загрузка waypoints из параметров ---
  loadWaypoints();

  // --- Подписка на GPS-одометрию от NavSat Transform ---
  // Диагностика: navsat_transform конвертирует GPS робота в
  // осях ENU (повёрнуты на 45° к map — известная особенность);
  // выводим для контроля модуля/направления вектора от старта.
  gps_odom_sub_ = this->create_subscription<nav_msgs::msg::Odometry>(
    "/odometry/gps", 10,
    std::bind(&GpsWaypointNode::gpsOdomCallback, this, std::placeholders::_1));

  // --- Подписка на сырые GPS-данные ---
  // Диагностика: контрольная сверка первого фикса с датумом.
  navsat_sub_ = this->create_subscription<sensor_msgs::msg::NavSatFix>(
    "/navsat", 10,
    std::bind(&GpsWaypointNode::navsatCallback, this, std::placeholders::_1));

  // --- Публикация текущей цели ---
  waypoint_pub_ = this->create_publisher<geometry_msgs::msg::PoseStamped>(
    "/gps_waypoint/current", 10);

  // --- Таймер (1 Гц): попытка конвертации + публикация ---
  timer_ = this->create_wall_timer(
    std::chrono::seconds(1),
    std::bind(&GpsWaypointNode::publishWaypoints, this));

  RCLCPP_INFO(this->get_logger(),
    "GPS Waypoint Node: загружено %zu waypoints, ожидание TF world->map...",
    waypoints_.size());
}

void GpsWaypointNode::loadWaypoints()
{
  // Общие параметры миссии
  this->declare_parameter<std::string>("frame_id", "map");
  this->declare_parameter<double>("timeout_sec", 300.0);

  // Датум ENU-проекции (центр мира Gazebo)
  this->declare_parameter<double>("datum_latitude", 55.755826);
  this->declare_parameter<double>("datum_longitude", 37.617300);
  datum_latitude_ = this->get_parameter("datum_latitude").as_double();
  datum_longitude_ = this->get_parameter("datum_longitude").as_double();

  // Количество точек маршрута
  this->declare_parameter<int>("num_waypoints", 2);
  int num_wp = this->get_parameter("num_waypoints").as_int();

  for (int i = 0; i < num_wp; i++) {
    GpsWaypoint wp;
    std::string prefix = "waypoints." + std::to_string(i);

    // Значения по умолчанию — датум (центр мира)
    this->declare_parameter<std::string>(prefix + ".name", "waypoint_" + std::to_string(i));
    this->declare_parameter<double>(prefix + ".latitude", datum_latitude_);
    this->declare_parameter<double>(prefix + ".longitude", datum_longitude_);
    this->declare_parameter<double>(prefix + ".altitude", 150.0);
    this->declare_parameter<double>(prefix + ".yaw", 0.0);
    this->declare_parameter<double>(prefix + ".tolerance", 1.0);

    wp.name = this->get_parameter(prefix + ".name").as_string();
    wp.latitude = this->get_parameter(prefix + ".latitude").as_double();
    wp.longitude = this->get_parameter(prefix + ".longitude").as_double();
    wp.altitude = this->get_parameter(prefix + ".altitude").as_double();
    wp.yaw = this->get_parameter(prefix + ".yaw").as_double();
    wp.tolerance = this->get_parameter(prefix + ".tolerance").as_double();

    waypoints_.push_back(wp);

    RCLCPP_INFO(this->get_logger(),
      "  Waypoint %d: '%s' lat=%.6f lon=%.6f yaw=%.2f",
      i, wp.name.c_str(), wp.latitude, wp.longitude, wp.yaw);
  }
}

void GpsWaypointNode::gpsOdomCallback(const nav_msgs::msg::Odometry::SharedPtr msg)
{
  // Диагностика (раз в 10 с): позиция по GPS (оси ENU, повёрнуты
  // на 45° к map). Сверять модуль/направление вектора от старта,
  // а не координаты напрямую.
  RCLCPP_INFO_THROTTLE(
    this->get_logger(), *this->get_clock(), 10000,
    "GPS-одометрия (map): x=%.2f, y=%.2f",
    msg->pose.pose.position.x, msg->pose.pose.position.y);
}

void GpsWaypointNode::navsatCallback(const sensor_msgs::msg::NavSatFix::SharedPtr msg)
{
  // Контрольная сверка ОДИН раз: первый GPS-фикс против датума.
  // Робот стоит в A (мир -7,-7) => ожидаем ENU-смещение ~(-7,-7) м.
  if (first_fix_logged_) {
    return;
  }
  first_fix_logged_ = true;

  double ex, ey;
  gpsToLocal(msg->latitude, msg->longitude, datum_latitude_, datum_longitude_, ex, ey);

  RCLCPP_INFO(this->get_logger(),
    "Первый GPS-фикс: lat=%.6f, lon=%.6f | ENU от датума: (%.2f, %.2f) м "
    "(ожидание ~ -7.0, -7.0 — старт A)",
    msg->latitude, msg->longitude, ex, ey);
}

void GpsWaypointNode::gpsToLocal(
  double lat, double lon,
  double ref_lat, double ref_lon,
  double & x, double & y) const
{
  // Equirectangular projection: точна для расстояний < 10 км,
  // для арены 20×20 м погрешность пренебрежима (< 0.001 мм).
  //
  //   x = (lon - ref_lon) * cos(ref_lat) * EARTH_RADIUS * DEG_TO_RAD
  //   y = (lat - ref_lat) * EARTH_RADIUS * DEG_TO_RAD
  //
  // x = Восток (East), y = Север (North) — система ENU.
  // Относительно датума (= центра мира) результат совпадает
  // с координатами мира Gazebo.

  double d_lat = (lat - ref_lat) * DEG_TO_RAD;
  double d_lon = (lon - ref_lon) * DEG_TO_RAD;
  double ref_lat_rad = ref_lat * DEG_TO_RAD;

  x = d_lon * std::cos(ref_lat_rad) * EARTH_RADIUS;  // Восток
  y = d_lat * EARTH_RADIUS;                          // Север
}

void GpsWaypointNode::convertWaypoints()
{
  // Конвертация возможна только при доступной TF world->map
  if (!tf_buffer_->canTransform("map", "world", tf2::TimePointZero)) {
    RCLCPP_INFO_THROTTLE(
      this->get_logger(), *this->get_clock(), 5000,
      "Ожидание статической TF world->map...");
    return;
  }

  RCLCPP_INFO(this->get_logger(), "Конвертация %zu waypoints в map frame...",
    waypoints_.size());

  converted_waypoints_.clear();

  for (const auto & wp : waypoints_) {
    // 1) GPS -> ENU относительно датума (= координаты мира Gazebo)
    double wx, wy;
    gpsToLocal(wp.latitude, wp.longitude, datum_latitude_, datum_longitude_, wx, wy);

    // 2) Мир -> map через статическую TF (сдвиг в A + поворот 45°)
    geometry_msgs::msg::PoseStamped world_pose;
    world_pose.header.frame_id = "world";
    world_pose.header.stamp = this->now();
    world_pose.pose.position.x = wx;
    world_pose.pose.position.y = wy;
    world_pose.pose.position.z = 0.0;  // 2D-навигация

    geometry_msgs::msg::PoseStamped map_pose;
    try {
      map_pose = tf_buffer_->transform(world_pose, "map", tf2::durationFromSec(0.5));
    } catch (const tf2::TransformException & ex) {
      RCLCPP_ERROR(this->get_logger(), "Ошибка TF world->map: %s", ex.what());
      return;
    }

    // 3) Ориентация — явно из параметра yaw (задаётся в map-фрейме):
    //    q = [0, 0, sin(yaw/2), cos(yaw/2)]
    map_pose.pose.orientation.x = 0.0;
    map_pose.pose.orientation.y = 0.0;
    map_pose.pose.orientation.z = std::sin(wp.yaw / 2.0);
    map_pose.pose.orientation.w = std::cos(wp.yaw / 2.0);

    converted_waypoints_.push_back(map_pose);

    RCLCPP_INFO(this->get_logger(),
      "  '%s': GPS(%.6f, %.6f) -> мир(%.2f, %.2f) -> map(%.2f, %.2f)",
      wp.name.c_str(), wp.latitude, wp.longitude,
      wx, wy, map_pose.pose.position.x, map_pose.pose.position.y);
  }

  is_ready_ = true;
  RCLCPP_INFO(this->get_logger(), "Конвертация завершена! Все waypoints готовы.");
}

void GpsWaypointNode::publishWaypoints()
{
  // Если конвертация ещё не выполнена — повторяем попытку
  if (!is_ready_) {
    convertWaypoints();
  }

  if (!is_ready_ || converted_waypoints_.empty()) {
    return;
  }

  // Публикуем первый waypoint как текущую цель (для RViz)
  auto current_wp = converted_waypoints_[0];
  current_wp.header.stamp = this->now();
  waypoint_pub_->publish(current_wp);
}

std::vector<geometry_msgs::msg::PoseStamped> GpsWaypointNode::getWaypoints() const
{
  return converted_waypoints_;
}

bool GpsWaypointNode::isReady() const
{
  return is_ready_;
}

}  // namespace scout_gps

// --- Точка входа (main) ---
int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);

  auto node = std::make_shared<scout_gps::GpsWaypointNode>();

  RCLCPP_INFO(node->get_logger(), "GPS Waypoint Node запущена.");

  rclcpp::spin(node);
  rclcpp::shutdown();

  return 0;
}
