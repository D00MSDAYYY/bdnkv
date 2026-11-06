#include "robot/robot.hpp"
#include "robot/simulated_camera.hpp"
#include "robot/simulated_drive_controller.hpp"
#include "robot/task_scheduler.hpp"

#include <chrono>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <thread>

int main() {
    using namespace std::chrono_literals;
    using robot::RobotState;
    try {
        robot::SimulatedCamera camera;
        robot::SimulatedDriveController controller;
        robot::Robot cart(camera, controller);
        robot::TaskScheduler scheduler; // Уничтожается раньше робота и оборудования.
        scheduler.add(20ms, [&] { controller.update(0.02); });
        scheduler.add(100ms, [&] { cart.update(); });

        bool started = false;
        const auto deadline = std::chrono::steady_clock::now() + 15s;
        while (true) {
            scheduler.rethrowIfFailed();
            const auto state = cart.status().state;
            if (state == RobotState::Completed || state == RobotState::Stopped || state == RobotState::Fault)
                break;
            if (state == RobotState::Ready && !started) {
                // Оператор задаёт маршрут и нажимает Пуск только после готовности.
                if (!cart.loadRoute(robot::Route({1, 2, 3})) || !cart.start())
                    throw std::runtime_error("Route start rejected");
                started = true;
            }
            if (std::chrono::steady_clock::now() >= deadline) cart.stop();
            if (std::chrono::steady_clock::now() >= deadline + 5s)
                throw std::runtime_error("Demo deadline exceeded");
            std::this_thread::sleep_for(20ms);
        }
        scheduler.stop();
        scheduler.rethrowIfFailed();

        const auto status = cart.status();
        std::cout << "Пройдено точек: " << status.visitedPoints << '/' << status.totalPoints << '\n';
        if (status.state == RobotState::Completed) std::cout << "Остановка подтверждена. Готово.\n";
        if (!status.faultReason.empty()) std::cerr << status.faultReason << '\n';
        return status.state == RobotState::Completed ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "Ошибка: " << error.what() << '\n';
        return 1;
    }
}
