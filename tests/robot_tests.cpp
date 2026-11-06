#include "robot/robot.hpp"

#include <chrono>
#include <iostream>
#include <stdexcept>

namespace {
using namespace std::chrono_literals;
using robot::RobotState;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

class TestCamera final : public robot::Camera {
public:
    bool online = false;
    bool canConnect = true;
    bool captureFails = false;
    robot::Frame frame{{{1, {1000, 0}}, {2, {2000, 0}}}};
    void connect() override { online = canConnect; }
    bool connected() const override { return online; }
    robot::Frame capture() override {
        if (captureFails) throw std::runtime_error("Camera capture failed");
        return frame;
    }
};

class TestDrive final : public robot::DriveController {
public:
    robot::Telemetry data;
    robot::Velocity command;
    bool autoReady = true;
    void connect() override { data.connected = true; confirmStop(); }
    void requestReady() override { data.ready = autoReady; }
    bool resetFault() override { data.watchdogExpired = false; data.ready = false; return true; }
    robot::Telemetry readTelemetry() const override { return data; }
    void setVelocity(robot::Velocity value) override {
        command = value;
        data.stoppedConfirmed = false;
    }
    void confirmStop() { data.measuredVelocity = {}; data.stoppedConfirmed = true; }
};

struct Fixture {
    TestCamera camera;
    TestDrive drive;
    robot::Robot cart{camera, drive};
    void ready() {
        cart.update();
        require(cart.status().state == RobotState::WaitingForReady, "Must wait for controller readiness");
        cart.update();
        require(cart.status().state == RobotState::Ready, "Must become ready before start");
    }
    void start() {
        ready();
        require(cart.loadRoute(robot::Route({1, 2})) && cart.start(), "Route must start after readiness");
    }
};

void checkRouteAndConfirmation() {
    Fixture f;
    require(!f.cart.start(), "Cannot start before initialization");
    f.ready();
    require(!f.cart.loadRoute(robot::Route{}), "Empty route must be rejected");
    require(f.cart.loadRoute(robot::Route({1, 2})) && f.cart.start(), "Valid route must start");
    f.cart.update();
    require(f.drive.command.linear > 0, "Robot must move towards first point");
    f.drive.data.pose.position = {1, 0};
    f.cart.update();
    require(f.cart.status().visitedPoints == 1, "Must count first point");
    f.drive.data.pose.position = {2, 0};
    f.cart.update();
    require(f.cart.status().state == RobotState::Stopping, "Last point must lead to Stopping");
    require(f.drive.command.linear == 0 && f.drive.command.angular == 0, "Must send Stop");
    f.cart.update();
    require(f.cart.status().state == RobotState::Stopping, "A sent command is not a stop confirmation");
    f.drive.confirmStop();
    f.cart.update();
    require(f.cart.status().state == RobotState::Completed, "Only confirmation may complete route");
    require(f.cart.status().stopReason == robot::StopReason::RouteCompleted, "Must remember stop reason");
    require(f.cart.loadRoute(robot::Route({2})), "New route must return Completed to Ready");
}

void checkLostMarkerAndOperatorStop() {
    Fixture f;
    f.start();
    f.cart.update();
    f.camera.frame.markerCandidates.clear();
    f.cart.update();
    require(f.drive.command.linear == 0 && f.cart.status().visitedPoints == 0, "Lost marker must preserve target and stop motion");
    require(f.cart.status().state == RobotState::Moving, "Lost marker is an internal transition");
    require(f.cart.stop(), "Operator stop must be accepted");
    require(f.cart.status().state == RobotState::Stopping, "Operator also waits for confirmation");
    f.drive.confirmStop();
    f.cart.update();
    require(f.cart.status().state == RobotState::Stopped, "Operator stop must not complete route");
    require(f.cart.status().stopReason == robot::StopReason::OperatorRequest, "Must distinguish operator stop");
    require(!f.cart.start(), "New route loading is required after Stopped");
}

void checkFaultAndReset() {
    Fixture f;
    f.start();
    f.drive.data.emergencyButton = true;
    f.cart.update();
    require(f.cart.status().state == RobotState::Fault, "Emergency input must cause Fault");
    f.drive.confirmStop();
    require(!f.cart.resetFault(), "Cannot reset an active emergency");
    require(!f.cart.start() && !f.cart.loadRoute(robot::Route({1})), "Fault must block route commands");
    f.drive.data.emergencyButton = false;
    f.cart.update();
    require(f.cart.status().state == RobotState::Fault, "Fault must remain latched");
    require(f.cart.resetFault(), "Explicit reset is accepted after cause is cleared");
    require(f.cart.status().state == RobotState::Initializing, "Reset must reinitialize");
    f.ready();
    require(!f.cart.start(), "Reset must not resume previous route");
    f.drive.data.obstacle = true;
    f.cart.update();
    require(f.cart.status().state == RobotState::Fault, "Safety must also work in Ready");
}

void checkTimeoutsAndHardwareErrors() {
    {
        Fixture f;
        f.camera.canConnect = false;
        f.cart.update();
        f.cart.update(std::chrono::steady_clock::now() + 3s);
        require(f.cart.status().state == RobotState::Fault, "Connection must have a timeout");
    }
    {
        Fixture f;
        f.drive.autoReady = false;
        f.cart.update();
        f.cart.update(std::chrono::steady_clock::now() + 3s);
        require(f.cart.status().state == RobotState::Fault, "Readiness must have a timeout");
    }
    {
        Fixture f;
        f.start();
        f.cart.stop();
        f.cart.update(std::chrono::steady_clock::now() + 3s);
        require(f.cart.status().state == RobotState::Fault, "Missing stop confirmation must cause Fault");
    }
    {
        Fixture f;
        f.start();
        f.camera.captureFails = true;
        f.cart.update();
        require(f.cart.status().state == RobotState::Fault && !f.cart.status().faultReason.empty(), "Hardware exception must be recorded as Fault");
    }
}
} // namespace

int main() {
    try {
        checkRouteAndConfirmation();
        checkLostMarkerAndOperatorStop();
        checkFaultAndReset();
        checkTimeoutsAndHardwareErrors();
        std::cout << "State machine checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
