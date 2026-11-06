#include "robot/route.hpp"

#include <utility>

namespace robot {

Route::Route(std::vector<int> pointIds) : pointIds_(std::move(pointIds)) {}

bool Route::completed() const { return next_ == pointIds_.size(); }

std::optional<int> Route::targetId() const {
    if (completed()) return std::nullopt;
    return pointIds_[next_];
}

void Route::advance() {
    if (!completed()) ++next_;
}

std::size_t Route::visited() const { return next_; }

std::size_t Route::size() const { return pointIds_.size(); }

} // namespace robot
