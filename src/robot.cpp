#include "robot/robot.hpp"

#include <algorithm>
#include <exception>
#include <utility>

namespace robot {
namespace {
bool unsafe(const Telemetry& data) {
    return data.obstacle || data.emergencyButton || data.watchdogExpired || data.deviceFault;
}
} // namespace

Robot::Robot(Camera& camera, DriveController& drive) : camera_(camera), drive_(drive) {}

Robot::~Robot() {
    try { drive_.stop(); } catch (...) { /* Связь с устройством может быть потеряна. */ }
}

void Robot::enter(RobotState state, Clock::time_point now) {
    state_ = state;
    stateSince_ = now;
}

void Robot::beginStopping(StopReason reason, Clock::time_point now) {
    stopReason_ = reason;
    enter(RobotState::Stopping, now);
    drive_.stop(); // entry / sendStop(). Ответ проверим на следующем tick.
}

void Robot::fail(std::string reason) {
    faultReason_ = std::move(reason);
    enter(RobotState::Fault);
    try { drive_.stop(); } catch (...) { /* Сохраняем исходную причину ошибки. */ }
}

bool Robot::equipmentReady() const {
    const auto data = drive_.readTelemetry();
    return camera_.connected() && data.connected && data.ready &&
           data.stoppedConfirmed && !unsafe(data);
}

bool Robot::loadRoute(Route route) {
    std::lock_guard lock(mutex_);
    if ((state_ != RobotState::Ready && state_ != RobotState::Completed &&
         state_ != RobotState::Stopped) || route.completed() || !equipmentReady()) return false;
    route_ = std::move(route);
    stopReason_ = StopReason::None;
    enter(RobotState::Ready);
    return true;
}

bool Robot::start() {
    std::lock_guard lock(mutex_);
    if (state_ != RobotState::Ready || route_.completed() || !equipmentReady()) return false;
    enter(RobotState::Moving);
    return true;
}

bool Robot::stop() {
    std::lock_guard lock(mutex_);
    if (state_ != RobotState::Moving) return false;
    beginStopping(StopReason::OperatorRequest);
    return true;
}

bool Robot::resetFault() {
    std::lock_guard lock(mutex_);
    if (state_ != RobotState::Fault) return false;
    const auto data = drive_.readTelemetry();
    if (!data.connected || !data.stoppedConfirmed || data.obstacle ||
        data.emergencyButton || data.deviceFault || !drive_.resetFault()) return false;
    route_ = Route{}; // После сброса нужен новый маршрут и отдельный Пуск.
    stopReason_ = StopReason::None;
    faultReason_.clear();
    connectionRequested_ = false;
    enter(RobotState::Initializing);
    return true;
}

void Robot::updateMotion(const Telemetry& data, Clock::time_point now) {
    const auto points = vision_.recognize(camera_.capture());
    const auto targetId = route_.targetId();
    const auto target = std::find_if(points.begin(), points.end(), [&](const auto& point) {
        return targetId && point.id == *targetId;
    });
    if (target == points.end()) {
        drive_.stop(); // Внутренний переход Moving: ждём метку, цель сохраняется.
    } else if (planner_.reached(data.pose, target->position)) {
        route_.advance();
        if (route_.completed()) beginStopping(StopReason::RouteCompleted, now);
    } else {
        drive_.setVelocity(planner_.towards(data.pose, target->position));
    }
}

void Robot::update(Clock::time_point now) {
    std::lock_guard lock(mutex_);
    if (state_ == RobotState::Fault) return;
    try {
        if (state_ == RobotState::Initializing && !connectionRequested_) {
            connectionRequested_ = true;
            camera_.connect();
            drive_.connect();
        }
        const auto data = drive_.readTelemetry();
        // Общий переход Operational -> Fault действует во всех рабочих состояниях.
        if (unsafe(data)) { fail("Safety input or controller fault"); return; }
        if (state_ != RobotState::Initializing && (!camera_.connected() || !data.connected)) {
            fail("Device connection lost");
            return;
        }
        if (state_ != RobotState::Initializing && state_ != RobotState::WaitingForReady && !data.ready) {
            fail("Controller is no longer ready");
            return;
        }
        const bool timedOut = now - stateSince_ >= phaseTimeout_;
        switch (state_) {
        case RobotState::Initializing:
            if (camera_.connected() && data.connected) {
                drive_.requestReady();
                enter(RobotState::WaitingForReady, now);
            } else if (timedOut) fail("Connection timeout");
            break;
        case RobotState::WaitingForReady:
            if (data.ready && data.stoppedConfirmed) enter(RobotState::Ready, now);
            else if (timedOut) fail("Readiness timeout");
            break;
        case RobotState::Moving:
            updateMotion(data, now);
            break;
        case RobotState::Stopping:
            if (data.stoppedConfirmed) {
                enter(stopReason_ == StopReason::RouteCompleted
                    ? RobotState::Completed : RobotState::Stopped, now);
            } else if (timedOut) fail("Stop confirmation timeout");
            break;
        case RobotState::Ready:
        case RobotState::Completed:
        case RobotState::Stopped:
        case RobotState::Fault:
            break;
        }
    } catch (const std::exception& error) { fail(error.what()); }
    catch (...) { fail("Unknown hardware error"); }
}

RobotStatus Robot::status() const {
    std::lock_guard lock(mutex_);
    return {state_, route_.visited(), route_.size(), stopReason_, faultReason_};
}

} // namespace robot
