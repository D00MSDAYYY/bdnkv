#include "robot/drive_controller.hpp"

namespace robot {

DriveController::~DriveController() = default;

void DriveController::stop() { setVelocity({}); }

} // namespace robot
