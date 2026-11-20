#ifndef AIRCOOBSERVER_H
#define AIRCOOBSERVER_H
#include <string>

class AircoObserver {
public:
    virtual ~AircoObserver() = default;
    virtual void onTargetTemperatureChange(int newTemperature) {};
    virtual void onCurrentTemperatureChange(int newTemperature) {};
    virtual void onStatusChange(std::string& newStatus) {};
};

#endif