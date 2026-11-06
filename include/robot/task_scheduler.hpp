#pragma once

#include <chrono>
#include <exception>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace robot {

// Для двух-трёх периодических функций достаточно одного потока на функцию.
// Это планировщик периодических вызовов, не планировщик жёсткого реального времени.
class TaskScheduler {
    std::mutex errorMutex_;
    std::exception_ptr error_;
    std::vector<std::jthread> workers_; // Уничтожаются раньше остальных полей.

public:
    ~TaskScheduler();

    // add() и stop() вызываются только из владеющего потока.
    void add(std::chrono::milliseconds period, std::function<void()> task);
    void stop();
    void rethrowIfFailed();
};

} // namespace robot
