#pragma once

#include "robot/drive_controller.hpp"

#include <chrono>
#include <mutex>

namespace robot {

// Учебная замена микроконтроллера: движение, одометрия и watchdog.
class SimulatedDriveController final : public DriveController {
    mutable std::mutex mutex_;
    Telemetry telemetry_;
    Velocity velocity_;
    std::chrono::steady_clock::time_point lastCommand_ = std::chrono::steady_clock::now();

public:
    void connect() override;
    void requestReady() override;
    bool resetFault() override;
    Telemetry readTelemetry() const override;
    void setVelocity(Velocity velocity) override;
    void update(double seconds);
};

} // namespace robot
