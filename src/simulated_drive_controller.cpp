#include "robot/simulated_drive_controller.hpp"

#include <cmath>

namespace robot {

void SimulatedDriveController::connect() {
    std::lock_guard lock(mutex_);
    telemetry_.connected = true;
    telemetry_.ready = false;
    velocity_ = {};
    lastCommand_ = std::chrono::steady_clock::now();
}

void SimulatedDriveController::requestReady() {
    std::lock_guard lock(mutex_);
    telemetry_.ready = telemetry_.connected && !telemetry_.obstacle &&
        !telemetry_.emergencyButton && !telemetry_.watchdogExpired && !telemetry_.deviceFault;
}

bool SimulatedDriveController::resetFault() {
    std::lock_guard lock(mutex_);
    if (!telemetry_.connected || !telemetry_.stoppedConfirmed || telemetry_.obstacle ||
        telemetry_.emergencyButton || telemetry_.deviceFault) return false;
    telemetry_.watchdogExpired = false;
    telemetry_.ready = false;
    return true;
}

Telemetry SimulatedDriveController::readTelemetry() const {
    std::lock_guard lock(mutex_);
    return telemetry_;
}

void SimulatedDriveController::setVelocity(Velocity velocity) {
    std::lock_guard lock(mutex_);
    const bool blocked = !telemetry_.connected || !telemetry_.ready || telemetry_.obstacle ||
        telemetry_.emergencyButton || telemetry_.watchdogExpired || telemetry_.deviceFault;
    velocity_ = blocked ? Velocity{} : velocity;
    telemetry_.stoppedConfirmed = false; // Старая телеметрия не подтверждает новую команду.
    lastCommand_ = std::chrono::steady_clock::now();
}

void SimulatedDriveController::update(double seconds) {
    std::lock_guard lock(mutex_);
    if (!telemetry_.connected) return;
    constexpr auto commandTimeout = std::chrono::milliseconds(500);
    const bool moving = velocity_.linear != 0 || velocity_.angular != 0;
    if (moving && std::chrono::steady_clock::now() - lastCommand_ > commandTimeout)
        telemetry_.watchdogExpired = true;

    // Контроллер блокирует приводы независимо от потока навигации.
    if (!telemetry_.ready || telemetry_.obstacle || telemetry_.emergencyButton ||
        telemetry_.watchdogExpired || telemetry_.deviceFault) velocity_ = {};

    telemetry_.measuredVelocity = velocity_; // Мгновенный отклик учебной модели.
    telemetry_.pose.heading += velocity_.angular * seconds;
    telemetry_.pose.position.x += velocity_.linear * std::cos(telemetry_.pose.heading) * seconds;
    telemetry_.pose.position.y += velocity_.linear * std::sin(telemetry_.pose.heading) * seconds;
    // Ответ появляется после обновления приводов, а не при отправке команды Стоп.
    telemetry_.stoppedConfirmed = velocity_.linear == 0 && velocity_.angular == 0;
}

} // namespace robot
