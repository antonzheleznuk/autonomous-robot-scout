/**
 * @file mission_manager.hpp
 * @brief Менеджер миссии автономного проезда по GPS-точкам
 *
 * Управляет логикой выполнения маршрута: ожидает готовности Nav2,
 * отправляет цели в action-сервер /navigate_to_pose, отслеживает
 * статус и публикует состояние миссии в топик /mission/status.
 *
 * Защита от preempt-гонки (реестр п.39): в этой среде goal-запрос
 * action обрабатывается сервером дважды (второй экземпляр вытесняет
 * первый за ~5-15 мс). Запрашивать результат мгновенно нельзя —
 * запрос цепляется к вытесняемому экземпляру и миссия получает
 * ложный ABORT. Поэтому результат запрашивается отложенно
 * (RESULT_DELAY_SEC), когда гонка на сервере уже решена; поздний
 * запрос корректен — Nav2 хранит результаты (result_timeout 900 с).
 *
 * Автор: Scout Mini Team
 * Дата: 2025
 */

#ifndef MISSION_MANAGER_HPP_
#define MISSION_MANAGER_HPP_

#include <rclcpp/rclcpp.hpp>
#include <rclcpp_action/rclcpp_action.hpp>
#include <nav2_msgs/action/navigate_to_pose.hpp>
#include <geometry_msgs/msg/pose_stamped.hpp>
#include <std_msgs/msg/string.hpp>

#include <future>
#include <string>
#include <vector>
#include <memory>

namespace scout_mission
{

enum class MissionState
{
  IDLE,                 // Начальное состояние / инициализация
  WAITING_FOR_NAV2,     // Ожидание готовности серверов Nav2
  SENDING_GOAL,         // Отправка очередной целевой точки
  NAVIGATING,           // Робот в пути к текущей цели
  POINT_REACHED,        // Точка достигнута
  RETRY_WAIT,           // Пауза перед повторной попыткой
  MISSION_COMPLETED,    // Все точки успешно пройдены (Финиш)
  MISSION_FAILED        // Ошибка выполнения миссии
};

class MissionManager : public rclcpp::Node
{
public:
  using NavigateToPose = nav2_msgs::action::NavigateToPose;
  using GoalHandleNav = rclcpp_action::ClientGoalHandle<NavigateToPose>;

  explicit MissionManager(const rclcpp::NodeOptions & options = rclcpp::NodeOptions());
  ~MissionManager() override = default;

private:
  // --- Основной цикл конечного автомата ---
  void controlLoop();

  // --- Методы работы с Action Client ---
  void sendNextGoal();
  void goalResponseCallback(const GoalHandleNav::SharedPtr & goal_handle);
  void feedbackCallback(
    GoalHandleNav::SharedPtr,
    const std::shared_ptr<const NavigateToPose::Feedback> feedback);
  void handleResult(const GoalHandleNav::WrappedResult & result);

  // --- Вспомогательные методы ---
  void scheduleRetry(const std::string & reason);
  void loadWaypointsFromParams();
  void publishStatus(const std::string & status_text);
  std::string stateToString(MissionState state) const;

  // --- Переменные состояния ---
  MissionState state_{MissionState::IDLE};
  std::vector<geometry_msgs::msg::PoseStamped> waypoints_;
  size_t current_waypoint_index_{0};

  // --- Контроль текущей цели и защиты от preempt-гонки (п.39) ---
  GoalHandleNav::SharedPtr current_goal_handle_;   // Активный goal handle
  std::shared_future<GoalHandleNav::WrappedResult> result_future_;
  bool result_requested_{false};                   // Результат уже запрошен
  rclcpp::Time goal_accepted_time_;                // Момент принятия цели
  int attempts_{0};                                // Попытка текущей точки
  rclcpp::Time retry_time_;                        // Когда разрешён повтор

  static constexpr int MAX_ATTEMPTS{5};            // Попыток на точку
  static constexpr double RESULT_DELAY_SEC{0.75};  // Задержка запроса результата
  static constexpr double RETRY_PAUSE_SEC{2.0};    // Пауза между попытками

  // Параметры
  std::string frame_id_{"map"};
  double mission_timeout_sec_{300.0};
  rclcpp::Time mission_start_time_;

  // --- ROS 2 интерфейсы ---
  rclcpp_action::Client<NavigateToPose>::SharedPtr nav_action_client_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr status_pub_;
  rclcpp::Publisher<geometry_msgs::msg::PoseStamped>::SharedPtr current_goal_pub_;
  rclcpp::TimerBase::SharedPtr loop_timer_;
};

}  // namespace scout_mission

#endif  // MISSION_MANAGER_HPP_
