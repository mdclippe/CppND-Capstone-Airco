#ifndef TEMPERATURE_NODE_H
#define TEMPERATURE_NODE_H
#include "Node.h"
#include <atomic>
#include <chrono>
#include <iostream>

// for simulation purposes
#include "AircoObserver.h"

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
    void onTargetTemperatureChange(int newTargetTemperature) override {
        _newTargetTemperature = newTargetTemperature;
    };
private:
    void cyclingTemperatures();
    std::atomic<int> _newTargetTemperature{0};
    std::chrono::milliseconds _stepInterval;
};

#endif
