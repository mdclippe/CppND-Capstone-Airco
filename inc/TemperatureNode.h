#ifndef TEMPERATURE_NODE_H
#define TEMPERATURE_NODE_H
#include "Node.h"
#include <iostream>

// for simulation purposes
#include "AircoObserver.h"

class TemperatureNode : public Node, public AircoObserver {
public :
    explicit TemperatureNode(std::shared_ptr<MessageQueue<int>> msgQueue, int id) : Node {msgQueue,id} {};
    ~TemperatureNode()
    {
        std::cout << "~TemperatureNode called" << std::endl;
    }
    void startSimulate() override;
    void stopSimulate() override;
    void onTargetTemperatureChange(int newTargetTemperature) override {
        _newTargetTemperature = newTargetTemperature;
    };
private:
    void cyclingTemperatures();
    int _newTargetTemperature = 0;
};

#endif