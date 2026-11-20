// System tests: the application wired like main.cpp with MQTT enabled, talking to a
// real broker. A separate "sensor" client publishes temperatures and a "dashboard"
// client follows the airco, like other devices on the network would.
// Skipped when no broker is reachable (see MqttBrokerTestSupport.h).
#include <gmock/gmock.h>

#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <string>

#include "Airco.h"
#include "AircoMqttBridge.h"
#include "MqttBrokerTestSupport.h"
#include "PahoMqttClient.h"
#include "TemperatureNode.h"
#include "TestHelpers.h"

using namespace std::chrono_literals;
using airco_test::testBrokerUri;
using airco_test::uniqueName;
using airco_test::waitUntil;

namespace {

// Last value seen per topic by the dashboard
class Dashboard
{
public:
    MqttClient::MessageHandler handler()
    {
        return [this](const std::string& topic, const std::string& payload) {
            std::lock_guard<std::mutex> lock(_mutex);
            _values[topic] = payload;
        };
    }
    std::string value(const std::string& topic) const
    {
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _values.find(topic);
        return it == _values.end() ? std::string() : it->second;
    }

private:
    mutable std::mutex _mutex;
    std::map<std::string, std::string> _values;
};

class AircoMqttSystemTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        SKIP_WITHOUT_MQTT_BROKER();

        // the application, as in main.cpp
        appClient = std::make_shared<PahoMqttClient>(testBrokerUri(), prefix + "-app");
        ASSERT_TRUE(appClient->connect());
        bridge = std::make_shared<AircoMqttBridge>(appClient, msgQueue, prefix);
        airco->addObserver(node1);
        airco->addObserver(bridge);
        airco->setTargetTemperature(20);
        node1->setValue(20);

        // the other devices on the network
        sensor = std::make_unique<PahoMqttClient>(testBrokerUri(), prefix + "-sensor");
        dashboardClient = std::make_unique<PahoMqttClient>(testBrokerUri(), prefix + "-dashboard");
        ASSERT_TRUE(sensor->connect());
        ASSERT_TRUE(dashboardClient->connect());
        for (const auto& topic : {topics.status, topics.targetTemperature, topics.currentTemperature})
        {
            dashboardClient->subscribe(topic, dashboard.handler());
        }
    }

    void TearDown() override
    {
        if (node1) node1->stopSimulate();
        if (dashboardClient && dashboardClient->isConnected())
        {
            // remove the retained messages of this test from the broker
            for (const auto& topic : {topics.status, topics.targetTemperature, topics.currentTemperature})
            {
                dashboardClient->publish(topic, "", true);
            }
        }
    }

    bool dashboardShows(const std::string& topic, const std::string& value)
    {
        return waitUntil([&] { return dashboard.value(topic) == value; }, 10s);
    }

    // publishes the reading until the airco used it (the app's subscription
    // may still be on its way when the test starts)
    bool sensorSends(const std::string& reading, const std::string& expectedCurrent)
    {
        return waitUntil([&] {
            sensor->publish(topics.sensorTemperature, reading, false);
            return dashboard.value(topics.currentTemperature) == expectedCurrent;
        }, 10s, 200ms);
    }

    const std::string prefix = uniqueName("airco-system-test");
    const AircoMqttBridge::Topics topics = AircoMqttBridge::topicsFor(prefix);

    std::shared_ptr<MessageQueue<int>> msgQueue = std::make_shared<MessageQueue<int>>(kTemperatureQueueCapacity);
    std::shared_ptr<TemperatureNode> node1 = std::make_shared<TemperatureNode>(msgQueue, 1);
    std::shared_ptr<Airco> airco = std::make_shared<Airco>(msgQueue);
    std::shared_ptr<PahoMqttClient> appClient;
    std::shared_ptr<AircoMqttBridge> bridge;

    std::unique_ptr<PahoMqttClient> sensor;
    std::unique_ptr<PahoMqttClient> dashboardClient;
    Dashboard dashboard;
};

} // namespace

TEST_F(AircoMqttSystemTest, AnMqttSensorDrivesTheAircoAndTheDashboardFollows)
{
    airco->togglePower();
    ASSERT_TRUE(dashboardShows(topics.status, "STANDBY"));
    ASSERT_TRUE(dashboardShows(topics.targetTemperature, "20"));

    ASSERT_TRUE(sensorSends("17.8", "18"));
    EXPECT_TRUE(dashboardShows(topics.status, "HEATING"));

    ASSERT_TRUE(sensorSends("20", "20"));
    EXPECT_TRUE(dashboardShows(topics.status, "STANDBY"));

    airco->togglePower();
    EXPECT_TRUE(dashboardShows(topics.status, "OFF"));
}

TEST_F(AircoMqttSystemTest, TheSimulatedRoomIsVisibleOnTheDashboard)
{
    airco->togglePower();
    node1->startSimulate();

    airco->setTargetTemperature(22);

    EXPECT_TRUE(dashboardShows(topics.targetTemperature, "22"));
    EXPECT_TRUE(dashboardShows(topics.status, "HEATING"));
    EXPECT_TRUE(dashboardShows(topics.currentTemperature, "22"));
    EXPECT_TRUE(dashboardShows(topics.status, "STANDBY"));
}
