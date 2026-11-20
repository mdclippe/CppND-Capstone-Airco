#ifndef TEMPERATURE_NODE_H
#define TEMPERATURE_NODE_H
#include "Node.h"
#include <atomic>
#include <chrono>
#include <iostream>
#include <string>

// for simulation purposes
#include "AircoObserver.h"

// Simulated room temperature sensor. The room follows what the airco does :
// it warms up one degree per step while the airco is HEATING, cools down one
// degree per step while it is COOLING and stays the same otherwise (STANDBY / OFF).
class TemperatureNode : public Node, public AircoObserver {
public :
    // stepInterval : time between two simulated temperature readings (tests use a shorter interval)
    explicit TemperatureNode(std::shared_ptr<MessageQueue<int>> msgQueue, int id,
                             std::chrono::milliseconds stepInterval = std::chrono::seconds(1))
        : Node {msgQueue,id}, _stepInterval(stepInterval) {};
    ~TemperatureNode()
    {
        // stop the thread before the members it uses are destroyed
        stopSimulate();
        std::cout << "~TemperatureNode called" << std::endl;
    }
    void startSimulate() override;
    void stopSimulate() override;
    void onStatusChange(std::string& newStatus) override;
private:
    void cyclingTemperatures();
    // degrees per simulation step : +1 heating, -1 cooling, 0 otherwise
    std::atomic<int> _change{0};
    std::chrono::milliseconds _stepInterval;
};

#endif
