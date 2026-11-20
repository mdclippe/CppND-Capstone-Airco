// Unit tests for Monitor: the bridge between Airco observer calls and Qt signals.
// Only built when the Qt GUI is enabled.
#include <gmock/gmock.h>

#include <memory>
#include <string>
#include <vector>

#include <QObject>
#include <QString>

#include "Monitor.h"

using ::testing::ElementsAre;

TEST(MonitorTest, TargetTemperatureChangeIsEmittedAsSignal)
{
    Monitor monitor;
    std::vector<double> emitted;
    QObject::connect(&monitor, &Monitor::targetTemperatureChanged,
                     [&emitted](double temperature) { emitted.push_back(temperature); });

    monitor.onTargetTemperatureChange(23);

    EXPECT_THAT(emitted, ElementsAre(23.0));
}

TEST(MonitorTest, CurrentTemperatureChangeIsEmittedAsSignal)
{
    Monitor monitor;
    std::vector<double> emitted;
    QObject::connect(&monitor, &Monitor::currentTemperatureChanged,
                     [&emitted](double temperature) { emitted.push_back(temperature); });

    monitor.onCurrentTemperatureChange(18);
    monitor.onCurrentTemperatureChange(19);

    EXPECT_THAT(emitted, ElementsAre(18.0, 19.0));
}

TEST(MonitorTest, StatusChangeIsEmittedAsQString)
{
    Monitor monitor;
    std::vector<std::string> emitted;
    QObject::connect(&monitor, &Monitor::aircoStatusChanged,
                     [&emitted](const QString& status) { emitted.push_back(status.toStdString()); });

    std::string status = "COOLING";
    monitor.onStatusChange(status);

    EXPECT_THAT(emitted, ElementsAre("COOLING"));
}

TEST(MonitorTest, ReactsWhenCalledThroughTheObserverInterface)
{
    auto monitor = std::make_shared<Monitor>();
    std::shared_ptr<AircoObserver> observer = monitor;
    std::vector<double> targets;
    QObject::connect(monitor.get(), &Monitor::targetTemperatureChanged,
                     [&targets](double temperature) { targets.push_back(temperature); });

    observer->onTargetTemperatureChange(21);

    EXPECT_THAT(targets, ElementsAre(21.0));
}
