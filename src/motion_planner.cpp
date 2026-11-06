#include "robot/motion_planner.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace robot {

bool MotionPlanner::reached(Pose pose, Point target) const {
    constexpr double tolerance = 0.02;
    return std::hypot(target.x - pose.position.x, target.y - pose.position.y) <= tolerance;
}

Velocity MotionPlanner::towards(Pose pose, Point target) const {
    const double dx = target.x - pose.position.x;
    const double dy = target.y - pose.position.y;
    const double distance = std::hypot(dx, dy);
    const double angleError = std::remainder(std::atan2(dy, dx) - pose.heading,
                                             2 * std::numbers::pi);
    // Сначала поворачиваемся к точке, затем едем с коррекцией направления.
    return {
        std::abs(angleError) < 0.3 ? std::min(distance * 2, 0.3) : 0.0,
        std::clamp(angleError * 3, -1.5, 1.5)
    };
}

} // namespace robot
