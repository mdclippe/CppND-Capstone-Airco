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
void TemperatureNode::cyclingTemperatures() {
    while (_running)
    {
        // startSimulate heating and cooling
        if (getValue() < _newTargetTemperature)
        {
            setValue(getValue()+1);
        }
        else if (getValue() > _newTargetTemperature)
        {
            setValue(getValue()-1);
        }
        _messageQueue->send(std::move(this->getValue()));
        std::this_thread::sleep_for(_stepInterval);
    }
}