#pragma once

#include "robot/camera.hpp"

namespace robot {

class SimulatedCamera final : public Camera {
    bool connected_ = false;
public:
    void connect() override;
    bool connected() const override;
    Frame capture() override;
};

} // namespace robot
