#pragma once

#include "robot/types.hpp"

namespace robot {

// Граница обмена Raspberry Pi с микроконтроллером.
// Реальная реализация будет отправлять команды по UART/CAN и читать телеметрию.
class DriveController {
public:
    virtual ~DriveController();
    virtual void connect() = 0;
    virtual void requestReady() = 0;
    virtual bool resetFault() = 0;
    virtual Telemetry readTelemetry() const = 0;
    // Подтверждение остановки придёт отдельно; старый stoppedConfirmed сбрасывается.
    virtual void setVelocity(Velocity velocity) = 0;
    void stop();
};

} // namespace robot
