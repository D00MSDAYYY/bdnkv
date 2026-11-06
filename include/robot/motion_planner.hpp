#pragma once

#include "robot/types.hpp"

namespace robot {

class MotionPlanner {
public:
    bool reached(Pose pose, Point target) const;
    Velocity towards(Pose pose, Point target) const;
};

} // namespace robot
