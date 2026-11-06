#include "robot/simulated_camera.hpp"

#include <stdexcept>

namespace robot {

void SimulatedCamera::connect() { connected_ = true; }

bool SimulatedCamera::connected() const { return connected_; }

Frame SimulatedCamera::capture() {
    if (!connected_) throw std::runtime_error("Camera is disconnected");
    // Фиксированная учебная сцена в пикселях. Захват USB-камеры опущен.
    return {{{1, {100, 0}}, {2, {100, 100}}, {3, {200, 100}}}};
}

} // namespace robot
