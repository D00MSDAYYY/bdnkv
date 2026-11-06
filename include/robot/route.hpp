#pragma once

#include <cstddef>
#include <optional>
#include <vector>

namespace robot {

// Маршрут хранит порядок идентификаторов точек, распознаваемых камерой.
class Route {
    std::vector<int> pointIds_;
    std::size_t next_ = 0;

public:
    explicit Route(std::vector<int> pointIds = {});
    bool completed() const;
    std::optional<int> targetId() const;
    void advance();
    std::size_t visited() const;
    std::size_t size() const;
};

} // namespace robot
