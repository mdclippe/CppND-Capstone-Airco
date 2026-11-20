// Integration tests: Airco + AircoMqttBridge + MessageQueue, with an in-memory
// MQTT broker standing in for Mosquitto (no network needed).
#include <gmock/gmock.h>

#include <chrono>
#include <memory>
#include <string>
#include <thread>

#include "Airco.h"
#include "AircoMqttBridge.h"
#include "FakeMqtt.h"
#include "TestHelpers.h"

using namespace std::chrono_literals;
using airco_test::FakeMqttBroker;
using airco_test::waitUntil;
using ::testing::Contains;

namespace {

class AircoMqttIntegrationTest : public ::testing::Test
{
protected:
    void SetUp() override { airco.addObserver(bridge); }

    // what an MQTT temperature sensor would do
    void sensorPublishes(const std::string& payload) { broker.publish(topics.sensorTemperature, payload); }

    // what an MQTT dashboard would see
    bool dashboardShows(const std::string& topic, const std::string& payload)
    {
        return waitUntil([&] { return broker.retainedMessage(topic) == payload; });
    }

    FakeMqttBroker broker;
    std::shared_ptr<MessageQueue<int>> queue = std::make_shared<MessageQueue<int>>(kTemperatureQueueCapacity);
    std::shared_ptr<AircoMqttBridge> bridge = std::make_shared<AircoMqttBridge>(broker.createClient(), queue);
    AircoMqttBridge::Topics topics = bridge->topics();
    Airco airco{queue};
};

} // namespace

TEST_F(AircoMqttIntegrationTest, PowerChangesArePublished)
{
    airco.togglePower();
    EXPECT_TRUE(dashboardShows(topics.status, "STANDBY"));

    airco.togglePower();
    EXPECT_TRUE(dashboardShows(topics.status, "OFF"));
}

TEST_F(AircoMqttIntegrationTest, TargetChangesArePublished)
{
    airco.setTargetTemperature(23);

    EXPECT_TRUE(dashboardShows(topics.targetTemperature, "23"));
}

TEST_F(AircoMqttIntegrationTest, AColdReadingFromAnMqttSensorMakesTheAircoHeat)
{
    airco.togglePower();

    sensorPublishes("17");

    EXPECT_TRUE(waitUntil([&] { return airco.getCurrentState() == AircoStates::HEATING; }));
    EXPECT_TRUE(dashboardShows(topics.currentTemperature, "17"));
    EXPECT_TRUE(dashboardShows(topics.status, "HEATING"));
}

TEST_F(AircoMqttIntegrationTest, ReachingTheTargetIsPublishedAsStandby)
{
    airco.togglePower();
    sensorPublishes("17");
    ASSERT_TRUE(dashboardShows(topics.status, "HEATING"));

    sensorPublishes("20.2"); // the target is 20

    EXPECT_TRUE(dashboardShows(topics.status, "STANDBY"));
    EXPECT_TRUE(dashboardShows(topics.currentTemperature, "20"));
}

TEST_F(AircoMqttIntegrationTest, InvalidReadingsDoNotDisturbTheAirco)
{
    airco.togglePower();
    sensorPublishes("17");
    ASSERT_TRUE(dashboardShows(topics.status, "HEATING"));

    sensorPublishes("very warm");
    sensorPublishes("25");

    EXPECT_TRUE(dashboardShows(topics.status, "COOLING"));
    EXPECT_THAT(broker.payloadsOn(topics.currentTemperature), ::testing::Not(Contains("very warm")));
}

TEST_F(AircoMqttIntegrationTest, ReadingsAreNotUsedWhileTheAircoIsOff)
{
    sensorPublishes("17");
    std::this_thread::sleep_for(100ms);

    EXPECT_EQ(AircoStates::OFF, airco.getCurrentState());
    EXPECT_TRUE(broker.payloadsOn(topics.currentTemperature).empty());
}
