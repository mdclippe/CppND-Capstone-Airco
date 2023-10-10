#ifndef AIRCO_H
#define AIRCO_H
#include "MessageQueue.h"
#include "AircoObserver.h"
#include <thread>
#include <mutex>
#include <vector>
#include <iostream>
#include <atomic>
#include <algorithm>


enum class AircoStates {OFF, COOLING, HEATING, STANDBYE};
enum class AircoModes {AUTO, MANUAL_HEAT, MANUAL_COOL};

class Airco {
public :
    explicit Airco(std::shared_ptr<MessageQueue<int>> messageQueue) : 
    _temperatureMessageQueue(messageQueue),
    _isOn(false), _currentTemperature(20), _state(AircoStates::OFF), _mode(AircoModes::AUTO)
     {
        _eventHandlingThread = std::make_unique<std::thread>(&Airco::eventHandlingLoop, this);
        _targetTemperature = 20;
        
    };
    ~Airco(){
        std::cout << "~Airco() called" << std::endl;
        _observers.clear();
        if (_runningTemperatureReadings && _temperatureReadingThread != nullptr)
        {
            _runningTemperatureReadings = false;
            _temperatureMessageQueue->unblock();
            _temperatureReadingThread->join();
        }
        if ( _runningEventHandler && _eventHandlingThread != nullptr)
        {
             _runningEventHandler = false;
             // need to unblock the waiting
             _eventQueue.unblock();
            _eventHandlingThread->join();
        }

    }
    void togglePower();
    void handleEvent(std::string event);

    void setTargetTemperature(int temperature);
    int getTargetTemperature() const {return this->_targetTemperature;}
    AircoStates getCurrentState() {
        std::lock_guard<std::mutex> lg(_mutex);
        return _state;
        }
    void setState(AircoStates newState) {
        std::lock_guard<std::mutex> lg(_mutex);
        if (_state != newState)
        {           
            _state = newState;
            std::string status = getStatus(_state);
            notifyStatusChange(status);
        }
    };

    void addObserver(AircoObserver* observer);
    void removeObserver(AircoObserver* observer);
    void notifyTargetTemperatureChange(int newTemperature);
    void notifyCurrentTemperatureChange(int newTemperature);
    void notifyStatusChange(std::string& status);

    void eventHandlingLoop();    
private:
    std::shared_ptr<MessageQueue<int>> _temperatureMessageQueue;
    std::vector<std::thread> _threads;
    std::unique_ptr<std::thread> _temperatureReadingThread;
    std::atomic<bool> _runningTemperatureReadings = true;

    MessageQueue<std::string> _eventQueue;
    
    std::unique_ptr<std::thread> _eventHandlingThread;
    std::atomic<bool> _runningEventHandler = true;

    std::vector<AircoObserver*> _observers;
    int _targetTemperature;
    int _currentTemperature;
    AircoStates _state;
    AircoModes _mode;
    bool _isOn;
    std::mutex _mutex;
    std::mutex _mode_mutex;
    
    void start();
    void stop();
    void updateTemperature();
    
    void updateState();
    void heat();
    void cool();
    void standbye();
    void setMode(AircoModes mode);
    std::string getStatus(AircoStates state) const;
};

#endif
