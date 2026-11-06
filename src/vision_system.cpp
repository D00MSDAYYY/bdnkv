#include "robot/vision_system.hpp"

namespace robot {

std::vector<FloorPoint> VisionSystem::recognize(const Frame& frame) const {
    auto points = frame.markerCandidates;
    // Упрощённая калибровка: 1000 пикселей = 1 метр, начало координат совпадает.
    // В реальном проекте здесь распознавание меток и преобразование камеры в пол.
    for (auto& point : points) {
        point.position.x *= 0.001;
        point.position.y *= 0.001;
    }
    return points;
}

} // namespace robot
