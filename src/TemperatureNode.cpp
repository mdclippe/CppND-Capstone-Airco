#include "TemperatureNode.h"

void TemperatureNode::startSimulate(){
    // send temperature values
    _running = true;
    _messageQueue->clear();
   _simulationThread = std::make_unique<std::thread>(&TemperatureNode::cyclingTemperatures, this);
}

void TemperatureNode::stopSimulate()
{
    if (_running && _simulationThread != nullptr)
    {
    _running = false;
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
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}