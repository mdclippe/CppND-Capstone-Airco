#include "TemperatureNode.h"

void TemperatureNode::startSimulate(){
    // already simulating : replacing the running thread would terminate the program
    if (_running.exchange(true))
    {
        return;
    }
    // send temperature values
    _messageQueue->clear();
   _simulationThread = std::make_unique<std::thread>(&TemperatureNode::cyclingTemperatures, this);
}

void TemperatureNode::stopSimulate()
{
    _running = false;
    if (_simulationThread != nullptr && _simulationThread->joinable())
    {
        _simulationThread->join();
    }
}
void TemperatureNode::onStatusChange(std::string& newStatus)
{
    if (newStatus == "HEATING")
    {
        _change = 1;
    }
    else if (newStatus == "COOLING")
    {
        _change = -1;
    }
    else
    {
        // STANDBY or OFF : nothing changes the room temperature
        _change = 0;
    }
}

void TemperatureNode::cyclingTemperatures() {
    while (_running)
    {
        // the room warms up or cools down only when the airco heats or cools
        setValue(getValue() + _change);
        _messageQueue->send(std::move(this->getValue()));
        std::this_thread::sleep_for(_stepInterval);
    }
}