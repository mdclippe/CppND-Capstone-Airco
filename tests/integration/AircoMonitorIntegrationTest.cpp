// Integration tests: Airco -> Monitor -> Qt signals.
// Only built when the Qt GUI is enabled.
#include <gmock/gmock.h>

#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <QObject>
#include <QString>

#include "Airco.h"
#include "Monitor.h"
#include "TestHelpers.h"

using airco_test::waitUntil;
using ::testing::Contains;
using ::testing::ElementsAre;

namespace {

class AircoMonitorIntegrationTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        airco.addObserver(monitor);

        // Direct connections: the lambdas run in the thread that emits,
        // which is an Airco worker thread for temperature readings.
        QObject::connect(monitor.get(), &Monitor::aircoStatusChanged, monitor.get(),
            [this](const QString& status) {
                std::lock_guard<std::mutex> lock(mutex);
                statuses.push_back(status.toStdString());
            }, Qt::DirectConnection);
        QObject::connect(monitor.get(), &Monitor::targetTemperatureChanged, monitor.get(),
            [this](double temperature) {
                std::lock_guard<std::mutex> lock(mutex);
                targets.push_back(temperature);
            }, Qt::DirectConnection);
        QObject::connect(monitor.get(), &Monitor::currentTemperatureChanged, monitor.get(),
            [this](double temperature) {
                std::lock_guard<std::mutex> lock(mutex);
                currents.push_back(temperature);
            }, Qt::DirectConnection);
    }

    template <class T>
    std::vector<T> snapshot(const std::vector<T>& values)
    {
        std::lock_guard<std::mutex> lock(mutex);
        return values;
    }

    std::shared_ptr<MessageQueue<int>> queue = std::make_shared<MessageQueue<int>>();
    std::shared_ptr<Monitor> monitor = std::make_shared<Monitor>();
    Airco airco{queue};

    std::mutex mutex;
    std::vector<std::string> statuses;
    std::vector<double> targets;
    std::vector<double> currents;
};

} // namespace

TEST_F(AircoMonitorIntegrationTest, PowerChangesAreEmittedAsStatusSignals)
{
    airco.togglePower();
    airco.togglePower();

    EXPECT_THAT(snapshot(statuses), ElementsAre("STANDBY", "OFF"));
}

TEST_F(AircoMonitorIntegrationTest, TargetTemperatureChangesAreEmitted)
{
    airco.setTargetTemperature(26);

    EXPECT_THAT(snapshot(targets), ElementsAre(26.0));
}

TEST_F(AircoMonitorIntegrationTest, ReadingsAndStateChangesFromTheWorkerThreadAreEmitted)
{
    airco.togglePower();

    queue->send(17);

    ASSERT_TRUE(waitUntil([&] {
        auto s = snapshot(statuses);
        return !s.empty() && s.back() == "HEATING";
    }));
    EXPECT_THAT(snapshot(currents), Contains(17.0));
    EXPECT_THAT(snapshot(statuses), ElementsAre("STANDBY", "HEATING"));
}
