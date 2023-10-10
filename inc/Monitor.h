#ifndef MONITOR_H
#define MONITOR_H

#include "Airco.h"
#include <iostream>
#include <thread>
#include <vector>
#include <QObject>



class Monitor : public QObject, public AircoObserver {
    Q_OBJECT
public:
    Monitor() {
        
    };
    ~Monitor() {
        std::cout << "~Monitor called" << std::endl;

    }   
    void onTargetTemperatureChange(int newTemperature)  override;
    void onCurrentTemperatureChange(int newTemperature)  override;
    void onStatusChange(std::string& newState) override;

signals:
    AircoStates getAircoState() const;
    void targetTemperatureChanged(double temp);
    void currentTemperatureChanged(double temp);
    void aircoStatusChanged(const QString& newState);
};

#endif