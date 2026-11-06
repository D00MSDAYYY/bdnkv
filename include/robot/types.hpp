#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace robot {

struct Point { double x = 0; double y = 0; }; // Метры.
struct Pose { Point position; double heading = 0; }; // Угол в радианах.
struct FloorPoint { int id; Point position; };

// Учебное представление кадра: кандидаты в метки после выделения областей.
// Их координаты пока заданы в пикселях; обработка сырых пикселей опущена.
struct Frame { std::vector<FloorPoint> markerCandidates; };

struct Velocity { double linear = 0; double angular = 0; }; // м/с и рад/с.
struct Telemetry {
    Pose pose; // Одометрия по энкодерам колёс.
    Velocity measuredVelocity;
    bool connected = false;
    bool ready = false;
    bool stoppedConfirmed = false; // Новая телеметрия подтверждает нулевую скорость.
    bool obstacle = false;
    bool emergencyButton = false;
    bool watchdogExpired = false;
    bool deviceFault = false;
};

enum class RobotState {
    Initializing, WaitingForReady, Ready, Moving,
    Stopping, Completed, Stopped, Fault
};

enum class StopReason { None, RouteCompleted, OperatorRequest };

struct RobotStatus {
    RobotState state;
    std::size_t visitedPoints;
    std::size_t totalPoints;
    StopReason stopReason;
    std::string faultReason;
};

} // namespace robot
