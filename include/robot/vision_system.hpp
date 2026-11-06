#pragma once

#include "robot/types.hpp"

#include <vector>

namespace robot {

class VisionSystem {
public:
    std::vector<FloorPoint> recognize(const Frame& frame) const;
};

} // namespace robot
