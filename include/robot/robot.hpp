#pragma once

#include "robot/camera.hpp"
#include "robot/drive_controller.hpp"
#include "robot/motion_planner.hpp"
#include "robot/route.hpp"
#include "robot/vision_system.hpp"

#include <chrono>
#include <mutex>

namespace robot {

// Оркестратор сценария: наблюдение -> цель -> движение -> следующая точка.
class Robot {
    using Clock = std::chrono::steady_clock;
    Camera& camera_;
    DriveController& drive_;
    VisionSystem vision_;
    MotionPlanner planner_;
    Route route_;
    RobotState state_ = RobotState::Initializing;
    StopReason stopReason_ = StopReason::None;
    std::string faultReason_;
    bool connectionRequested_ = false;
    Clock::time_point stateSince_ = Clock::now();
    const std::chrono::milliseconds phaseTimeout_{2000};
    mutable std::mutex mutex_; // start/stop/status и периодический update.

    void enter(RobotState state, Clock::time_point now = Clock::now());
    void beginStopping(StopReason reason, Clock::time_point now = Clock::now());
    void fail(std::string reason);
    bool equipmentReady() const;
    void updateMotion(const Telemetry& telemetry, Clock::time_point now);

public:
    Robot(Camera& camera, DriveController& drive);
    ~Robot();
    bool loadRoute(Route route);
    bool start();
    bool stop();
    bool resetFault();
    // Обычно время берётся автоматически; в тестах можно проверить тайм-аут без sleep.
    void update(Clock::time_point now = Clock::now());
    RobotStatus status() const;
};

} // namespace robot
