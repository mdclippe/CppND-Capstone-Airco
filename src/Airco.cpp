#include "Airco.h"
#include "AircoObserver.h"
#include <iostream>
#include <thread>
#include <vector>

void Airco::setTargetTemperature(int temperature)
{
    std::lock_guard<std::mutex> lg(_mutex);
    _targetTemperature = temperature;
    notifyTargetTemperatureChange(temperature);
}

void Airco::heat()
{
    setState(AircoStates::HEATING);
}

void Airco::cool()
{
    setState(AircoStates::COOLING);
}

void Airco::standbye()
{
    setState(AircoStates::STANDBYE);
}

void Airco::setMode(AircoModes mode)
{
    std::lock_guard lg(_mode_mutex);
    _mode = mode;
}

void Airco::togglePower()
{
    _isOn = !_isOn;
    if (_isOn)
    {
        setState(AircoStates::STANDBYE);
        start();
    }
    else
    {
        setState(AircoStates::OFF);
        stop();
    }
}

void Airco::stop()
{
    if (_runningTemperatureReadings && _temperatureReadingThread != nullptr)
    {
        _runningTemperatureReadings = false;
        _temperatureMessageQueue->unblock();    
        _temperatureReadingThread->join();
    }
}

void Airco::start()
{
    _runningTemperatureReadings = true;
    _temperatureMessageQueue->clear();
    _temperatureReadingThread = std::make_unique<std::thread>(&Airco::updateTemperature, this);
}

void Airco::updateTemperature()
{
    while (_runningTemperatureReadings)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        // get from the queue
        auto temperature = _temperatureMessageQueue->receive();
        if (temperature.has_value())
        {
            _currentTemperature = temperature.value();
            notifyCurrentTemperatureChange(_currentTemperature);
        }
        if (getCurrentState() != AircoStates::OFF)
        {
            updateState();
        }
    }
}

void Airco::updateState()
{
    std::lock_guard<std::mutex> lg(_mode_mutex);
    if (_mode == AircoModes::AUTO)
    {
        if (_currentTemperature < _targetTemperature)
        {
            heat();
        }
        else if (_currentTemperature > _targetTemperature)
        {
            cool();
        }
        else
        {
            standbye();
        }
    }
    else
    {
        switch (_mode)
        {
            case AircoModes::MANUAL_HEAT :
            {
                if (_currentTemperature < _targetTemperature)
                {
                    heat();
                }   
                else
                {
                    standbye();
                }
                break;
            }
            case AircoModes::MANUAL_COOL :
            {
                if (_currentTemperature > _targetTemperature)
                {
                    cool();
                }   
                else
                {
                    standbye();
                }
                break;
            }
            default :
            {
                standbye();
                break;
            }
        }
    }
}

void Airco::handleEvent(std::string event) {
    _eventQueue.send(std::move(event));
}
void Airco::eventHandlingLoop() {
    while (_runningEventHandler) 
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        auto event = _eventQueue.receive();
        if (event.has_value())
        {
            if (event.value() == "cooling_requested") {
                setMode(AircoModes::MANUAL_COOL);
                std::cout << "Switching to cooling mode." << std::endl;
            } else if (event.value() == "heating_requested") {
                setMode(AircoModes::MANUAL_HEAT);
                std::cout << "Switching to heating mode." << std::endl;
            } else if (event.value() == "auto_requested")
            {
                setMode(AircoModes::AUTO);
                std::cout << "switching to Auto mode." << std::endl;
            }
        }
    }
}
void Airco::addObserver(std::shared_ptr<AircoObserver> observer) {
    _observers.emplace_back(observer);
}

void Airco::removeObserver(std::shared_ptr<AircoObserver> observer) {
    // Implement observer removal logic if needed
    auto it = std::find(_observers.begin(), _observers.end(), observer);
    if (it != _observers.end()) {
           _observers.erase(it);
    }
}

void Airco::notifyTargetTemperatureChange(int newTemperature) {
    for (auto observer : _observers) {
        observer->onTargetTemperatureChange(newTemperature);
    }
}

void Airco::notifyCurrentTemperatureChange(int newTemperature) {
    for (auto observer : _observers) {
        observer->onCurrentTemperatureChange(newTemperature);
    }
}


void Airco::notifyStatusChange(std::string& status) {
    for (auto observer : _observers) {
        observer->onStatusChange(status);
    }
}

std::string Airco::getStatus(AircoStates state) const {
    std::string status;
    switch (state) {
        case AircoStates::COOLING :
        {
            status = "COOLING";
            break;
        }
        case AircoStates::HEATING :
        {
            status = "HEATING";
            break;
        }
        case AircoStates::STANDBYE :
        {
            status = "STANDBYE";
            break;
        }
        case AircoStates::OFF:
        {
            status = "OFF";
            break;
        }
    }
    return status;
}



