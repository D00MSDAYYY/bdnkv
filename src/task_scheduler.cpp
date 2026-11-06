#include "robot/task_scheduler.hpp"

#include <condition_variable>
#include <stdexcept>
#include <utility>

namespace robot {

TaskScheduler::~TaskScheduler() { stop(); }

void TaskScheduler::add(std::chrono::milliseconds period, std::function<void()> task) {
    if (period.count() <= 0 || !task) throw std::invalid_argument("Invalid periodic task");
    workers_.emplace_back([this, period, task = std::move(task)](std::stop_token stop) {
        try {
            std::mutex mutex;
            std::condition_variable_any wake;
            std::unique_lock lock(mutex);
            while (!stop.stop_requested()) {
                const auto next = std::chrono::steady_clock::now() + period;
                task();
                wake.wait_until(lock, stop, next, [] { return false; });
            }
        } catch (...) {
            std::lock_guard lock(errorMutex_);
            if (!error_) error_ = std::current_exception();
        }
    });
}

void TaskScheduler::stop() {
    for (auto& worker : workers_) worker.request_stop();
    workers_.clear(); // jthread дожидается завершения каждого вызова.
}

void TaskScheduler::rethrowIfFailed() {
    std::lock_guard lock(errorMutex_);
    if (error_) std::rethrow_exception(error_);
}

} // namespace robot
