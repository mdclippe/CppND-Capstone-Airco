#include "Monitor.h"
#include <iostream>


void Monitor::onTargetTemperatureChange(int newTemperature)  {
    emit targetTemperatureChanged(newTemperature);
};

void Monitor::onCurrentTemperatureChange(int newTemperature)  {
    emit currentTemperatureChanged(newTemperature);
};

void Monitor::onStatusChange(std::string& newStatus) {
    emit aircoStatusChanged(QString::fromStdString(newStatus));
}

