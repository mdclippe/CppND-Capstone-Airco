#ifndef AIRCOOBSERVER_H
#define AIRCOOBSERVER_H

class AircoObserver {
public:
    virtual void onTargetTemperatureChange(int newTemperature) {};
    virtual void onCurrentTemperatureChange(int newTemperature) {};
    virtual void onStatusChange(std::string& newStatus) {};
};

#endif