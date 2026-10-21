/**
 * @file mission_manager.cpp
 * @brief Реализация менеджера миссии Scout Mini
 *
 * Маршрут: единственная цель — Финиш B (19.8, 0) в map-фрейме
 * проекта. Точка старта A НЕ отправляется: робот спавнится в A,
 * а нулевая дистанция цели порождала мгновенную гонку preempt
 * (реестр п.39).
 */

#include "mission_manager.hpp"
#include <cstdlib>  // getenv: версия проекта из SCOUT_VERSION

namespace scout_mission
{

MissionManager::MissionManager(const rclcpp::NodeOptions & options)
: Node("mission_manager", options)
{
  const char * project_ver = std::getenv("SCOUT_VERSION");
  RCLCPP_INFO(this->get_logger(), "=== Mission Manager v%s: Инициализация ===",
    project_ver ? project_ver : "dev");

  this->declare_parameter<std::string>("frame_id", "map");
  this->declare_parameter<double>("timeout_sec", 300.0);
  frame_id_ = this->get_parameter("frame_id").as_string();
  mission_timeout_sec_ = this->get_parameter("timeout_sec").as_double();

  loadWaypointsFromParams();

  nav_action_client_ = rclcpp_action::create_client<NavigateToPose>(
    this, "/navigate_to_pose");

  status_pub_ = this->create_publisher<std_msgs::msg::String>("/mission/status", 10);
  current_goal_pub_ = this->create_publisher<geometry_msgs::msg::PoseStamped>(
    "/goal_pose", 10);

  loop_timer_ = this->create_wall_timer(
    std::chrono::milliseconds(100),
    std::bind(&MissionManager::controlLoop, this));

  state_ = MissionState::WAITING_FOR_NAV2;

  RCLCPP_INFO(this->get_logger(), "Mission Manager запущен. Ожидание готовности Nav2...");
}

void MissionManager::loadWaypointsFromParams()
{
  // Единственная цель — Финиш B в map-фрейме (реестр п.5, п.39):
  // 19.8 = 14*sqrt(2) — диагональ мира из A(-7,-7) в B(7,7).
  // Точка A исключена: робот спавнится в ней, а нулевая цель
  // завершалась за миллисекунды и усиливала preempt-гонку.
  this->declare_parameter<int>("num_waypoints", 1);
  int num_wp = this->get_parameter("num_waypoints").as_int();

  std::vector<std::pair<double, double>> coords = {{19.8, 0.0}};
  std::vector<double> yaws = {0.0};

  for (int i = 0; i < num_wp && i < static_cast<int>(coords.size()); ++i) {
    geometry_msgs::msg::PoseStamped p;
    p.header.frame_id = frame_id_;
    p.pose.position.x = coords[i].first;
    p.pose.position.y = coords[i].second;
    p.pose.position.z = 0.0;
    p.pose.orientation.z = std::sin(yaws[i] / 2.0);
    p.pose.orientation.w = std::cos(yaws[i] / 2.0);
    waypoints_.push_back(p);

    RCLCPP_INFO(this->get_logger(), "Загружена точка %d: x=%.2f, y=%.2f (map frame)",
      i + 1, p.pose.position.x, p.pose.position.y);
  }
}

void MissionManager::controlLoop()
{
  // Глобальный таймаут миссии (только в фазе движения)
  if (state_ == MissionState::NAVIGATING) {
    double elapsed = (this->now() - mission_start_time_).seconds();
    if (elapsed > mission_timeout_sec_) {
      RCLCPP_ERROR(this->get_logger(), "ПРЕВЫШЕН ТАЙМАУТ МИССИИ (%.1f с)!", mission_timeout_sec_);
      state_ = MissionState::MISSION_FAILED;
      publishStatus("FAILED: TIMEOUT");
      return;
    }
  }

  switch (state_)
  {
    case MissionState::WAITING_FOR_NAV2:
    {
      publishStatus("WAITING_FOR_NAV2");
      if (nav_action_client_->action_server_is_ready()) {
        RCLCPP_INFO(this->get_logger(), "Action-сервер /navigate_to_pose готов к приёму целей!");
        mission_start_time_ = this->now();
        state_ = MissionState::SENDING_GOAL;
      }
      break;
    }

    case MissionState::SENDING_GOAL:
    {
      if (current_waypoint_index_ >= waypoints_.size()) {
        RCLCPP_INFO(this->get_logger(), "Все точки пройдены! Миссия успешно завершена.");
        state_ = MissionState::MISSION_COMPLETED;
        publishStatus("MISSION_COMPLETED: ARRIVED AT POINT B");
      } else {
        sendNextGoal();
      }
      break;
    }

    case MissionState::NAVIGATING:
    {
      publishStatus("NAVIGATING_TO_WAYPOINT_" + std::to_string(current_waypoint_index_ + 1));

      // --- Отложенный запрос результата (реестр п.39) ---
      // Результат НЕ запрашивается при принятии цели: в первые
      // ~15 мс на сервере идёт preempt-шторм дубля, и мгновенный
      // запрос цепляется к вытесняемому экземпляру (ложный ABORT).
      // Через RESULT_DELAY_SEC гонка решена; поздний запрос
      // получает итог выжившего экземпляра (Nav2 хранит результаты).
      if (!result_requested_ && current_goal_handle_ &&
        (this->now() - goal_accepted_time_).seconds() > RESULT_DELAY_SEC)
      {
        try {
          result_future_ = nav_action_client_->async_get_result(current_goal_handle_);
          result_requested_ = true;
          RCLCPP_INFO(this->get_logger(),
            "Запрос результата цели %zu отправлен (отложенно)",
            current_waypoint_index_ + 1);
        } catch (const std::exception & e) {
          RCLCPP_WARN(this->get_logger(), "Не удалось запросить результат: %s", e.what());
          scheduleRetry("ошибка запроса результата");
          return;
        }
      }

      // --- Неблокирующая проверка готовности результата ---
      if (result_requested_ &&
        result_future_.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
      {
        handleResult(result_future_.get());
      }
      break;
    }

    case MissionState::POINT_REACHED:
    {
      RCLCPP_INFO(this->get_logger(), "Точка %zu достигнута!", current_waypoint_index_ + 1);
      current_waypoint_index_++;
      attempts_ = 0;
      state_ = MissionState::SENDING_GOAL;
      break;
    }

    case MissionState::RETRY_WAIT:
    {
      publishStatus("RETRY_WAIT");
      if (this->now() >= retry_time_) {
        state_ = MissionState::SENDING_GOAL;
      }
      break;
    }

    case MissionState::MISSION_COMPLETED:
    {
      publishStatus("ARRIVED");
      break;
    }

    case MissionState::MISSION_FAILED:
    {
      publishStatus("FAILED");
      break;
    }

    default:
      break;
  }
}

void MissionManager::sendNextGoal()
{
  auto target_pose = waypoints_[current_waypoint_index_];
  target_pose.header.stamp = this->now();

  attempts_++;
  RCLCPP_INFO(this->get_logger(), ">>> Отправка цели %zu (попытка %d): x=%.2f, y=%.2f",
    current_waypoint_index_ + 1, attempts_,
    target_pose.pose.position.x, target_pose.pose.position.y);

  current_goal_pub_->publish(target_pose);

  NavigateToPose::Goal goal;
  goal.pose = target_pose;

  auto send_goal_options = rclcpp_action::Client<NavigateToPose>::SendGoalOptions();

  send_goal_options.goal_response_callback =
    std::bind(&MissionManager::goalResponseCallback, this, std::placeholders::_1);

  send_goal_options.feedback_callback =
    std::bind(&MissionManager::feedbackCallback, this, std::placeholders::_1, std::placeholders::_2);

  // result_callback НАМЕРЕННО не задаётся: результат запрашивается
  // отложенно из controlLoop (п.39)

  // Сброс контроля предыдущей цели
  current_goal_handle_ = nullptr;
  result_requested_ = false;
  result_future_ = std::shared_future<GoalHandleNav::WrappedResult>();

  nav_action_client_->async_send_goal(goal, send_goal_options);
  state_ = MissionState::NAVIGATING;
}

void MissionManager::goalResponseCallback(const GoalHandleNav::SharedPtr & goal_handle)
{
  if (!goal_handle) {
    RCLCPP_ERROR(this->get_logger(), "Цель отклонена Nav2!");
    scheduleRetry("цель отклонена");
    return;
  }
  current_goal_handle_ = goal_handle;
  goal_accepted_time_ = this->now();
  result_requested_ = false;
  RCLCPP_INFO(this->get_logger(), "Цель принята Nav2 (попытка %d).", attempts_);
}

void MissionManager::feedbackCallback(
  GoalHandleNav::SharedPtr,
  const std::shared_ptr<const NavigateToPose::Feedback> feedback)
{
  RCLCPP_INFO_THROTTLE(
    this->get_logger(), *this->get_clock(), 2000,
    "Осталось до цели: %.2f м | Время в пути: %.1f с",
    feedback->distance_remaining,
    feedback->navigation_time.sec + feedback->navigation_time.nanosec * 1e-9);
}

void MissionManager::handleResult(const GoalHandleNav::WrappedResult & result)
{
  switch (result.code) {
    case rclcpp_action::ResultCode::SUCCEEDED:
      RCLCPP_INFO(this->get_logger(), "Цель успешно достигнута!");
      state_ = MissionState::POINT_REACHED;
      break;
    case rclcpp_action::ResultCode::ABORTED:
      RCLCPP_ERROR(this->get_logger(), "Движение к цели прервано (Aborted)!");
      scheduleRetry("Aborted");
      break;
    case rclcpp_action::ResultCode::CANCELED:
      RCLCPP_WARN(this->get_logger(), "Движение к цели отменено (Canceled)!");
      scheduleRetry("Canceled");
      break;
    default:
      RCLCPP_ERROR(this->get_logger(), "Неизвестный код результата Action Server");
      scheduleRetry("неизвестный код результата");
      break;
  }
}

void MissionManager::scheduleRetry(const std::string & reason)
{
  if (attempts_ >= MAX_ATTEMPTS) {
    RCLCPP_ERROR(this->get_logger(),
      "Исчерпаны попытки (%d) для точки %zu: %s. Миссия провалена.",
      attempts_, current_waypoint_index_ + 1, reason.c_str());
    state_ = MissionState::MISSION_FAILED;
    publishStatus("FAILED: " + reason);
    return;
  }
  RCLCPP_WARN(this->get_logger(),
    "Повтор через %.1f с (попытка %d/%d): %s",
    RETRY_PAUSE_SEC, attempts_, MAX_ATTEMPTS, reason.c_str());
  retry_time_ = this->now() + rclcpp::Duration::from_seconds(RETRY_PAUSE_SEC);
  state_ = MissionState::RETRY_WAIT;
}

void MissionManager::publishStatus(const std::string & status_text)
{
  std_msgs::msg::String msg;
  msg.data = status_text;
  status_pub_->publish(msg);
}

std::string MissionManager::stateToString(MissionState state) const
{
  switch (state) {
    case MissionState::IDLE: return "IDLE";
    case MissionState::WAITING_FOR_NAV2: return "WAITING_FOR_NAV2";
    case MissionState::SENDING_GOAL: return "SENDING_GOAL";
    case MissionState::NAVIGATING: return "NAVIGATING";
    case MissionState::POINT_REACHED: return "POINT_REACHED";
    case MissionState::RETRY_WAIT: return "RETRY_WAIT";
    case MissionState::MISSION_COMPLETED: return "MISSION_COMPLETED";
    case MissionState::MISSION_FAILED: return "MISSION_FAILED";
    default: return "UNKNOWN";
  }
}

}  // namespace scout_mission

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<scout_mission::MissionManager>();
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
