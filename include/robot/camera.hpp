#pragma once

#include "robot/types.hpp"

namespace robot {

class Camera {
public:
    virtual ~Camera();
    virtual void connect() = 0;
    virtual bool connected() const = 0;
    virtual Frame capture() = 0;
};

} // namespace robot
