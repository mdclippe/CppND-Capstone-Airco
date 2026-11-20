// Unit tests for AircoMqttBridge: MQTT client replaced by a gMock mock or an in-memory broker.
#include <gmock/gmock.h>

#include <chrono>
#include <memory>
#include <optional>
#include <string>

#include "AircoMqttBridge.h"
#include "FakeMqtt.h"
#include "TestHelpers.h"

using namespace std::chrono_literals;
using airco_test::FakeMqttBroker;
using airco_test::MockMqttClient;
using airco_test::receiveWithin;
using ::testing::_;
using ::testing::NiceMock;

// ---------------------------------------------------------------------------
// Parsing the temperature payload
// ---------------------------------------------------------------------------

TEST(AircoMqttBridgeParseTest, AcceptsWholeNumbers)
{
    EXPECT_EQ(21, AircoMqttBridge::parseTemperature("21"));
    EXPECT_EQ(0, AircoMqttBridge::parseTemperature("0"));
    EXPECT_EQ(-3, AircoMqttBridge::parseTemperature("-3"));
    EXPECT_EQ(5, AircoMqttBridge::parseTemperature("+5"));
}

TEST(AircoMqttBridgeParseTest, IgnoresSurroundingWhitespace)
{
    EXPECT_EQ(22, AircoMqttBridge::parseTemperature(" 22"));
    EXPECT_EQ(22, AircoMqttBridge::parseTemperature("22\n"));
}

TEST(AircoMqttBridgeParseTest, RoundsDecimalsToTheNearestDegree)
{
    EXPECT_EQ(21, AircoMqttBridge::parseTemperature("21.4"));
    EXPECT_EQ(22, AircoMqttBridge::parseTemperature("21.6"));
    EXPECT_EQ(22, AircoMqttBridge::parseTemperature("21.5"));
    EXPECT_EQ(-3, AircoMqttBridge::parseTemperature("-2.5"));
}

TEST(AircoMqttBridgeParseTest, AcceptsTheLimitsOfThePlausibleRange)
{
    EXPECT_EQ(AircoMqttBridge::kMinTemperature,
              AircoMqttBridge::parseTemperature(std::to_string(AircoMqttBridge::kMinTemperature)));
    EXPECT_EQ(AircoMqttBridge::kMaxTemperature,
              AircoMqttBridge::parseTemperature(std::to_string(AircoMqttBridge::kMaxTemperature)));
}

class AircoMqttBridgeInvalidPayloadTest : public ::testing::TestWithParam<const char*> {};

TEST_P(AircoMqttBridgeInvalidPayloadTest, IsRejected)
{
    const auto temperature = AircoMqttBridge::parseTemperature(GetParam());
    EXPECT_FALSE(temperature.has_value()) << "parsed as " << temperature.value_or(0);
}

INSTANTIATE_TEST_SUITE_P(Payloads, AircoMqttBridgeInvalidPayloadTest,
    ::testing::Values("",          // empty
                      "   ",       // only whitespace
                      "warm",      // no number
                      "21abc",     // trailing text
                      "21,5",      // decimal comma
                      "21 22",     // two numbers
                      "0x15",      // hexadecimal
                      "nan",
                      "inf",
                      "1e10",      // exponent notation
                      "+",         // sign without digits
                      ".",         // point without digits
                      "21.5.1",    // two decimal points
                      "999999",    // far out of range
                      "101",       // above kMaxTemperature
                      "-101"));    // below kMinTemperature

// ---------------------------------------------------------------------------
// Topics
// ---------------------------------------------------------------------------

TEST(AircoMqttBridgeTopicsTest, AllTopicsAreBelowThePrefix)
{
    const auto topics = AircoMqttBridge::topicsFor("home/living");

    EXPECT_EQ("home/living/sensor/temperature", topics.sensorTemperature);
    EXPECT_EQ("home/living/status", topics.status);
    EXPECT_EQ("home/living/target_temperature", topics.targetTemperature);
    EXPECT_EQ("home/living/current_temperature", topics.currentTemperature);
}

TEST(AircoMqttBridgeTopicsTest, DefaultPrefixIsAirco)
{
    auto client = std::make_shared<NiceMock<MockMqttClient>>();
    AircoMqttBridge bridge(client, std::make_shared<MessageQueue<int>>());

    EXPECT_EQ("airco/sensor/temperature", bridge.topics().sensorTemperature);
}

// ---------------------------------------------------------------------------
// Publishing (verified with the gMock client)
// ---------------------------------------------------------------------------

namespace {

class AircoMqttBridgePublishTest : public ::testing::Test
{
protected:
    std::shared_ptr<NiceMock<MockMqttClient>> client = std::make_shared<NiceMock<MockMqttClient>>();
    AircoMqttBridge bridge{client, std::make_shared<MessageQueue<int>>()};
};

} // namespace

TEST_F(AircoMqttBridgePublishTest, SubscribesToTheSensorTopicWhenCreated)
{
    auto otherClient = std::make_shared<MockMqttClient>();
    EXPECT_CALL(*otherClient, subscribe("airco/sensor/temperature", _)).Times(1);

    AircoMqttBridge otherBridge(otherClient, std::make_shared<MessageQueue<int>>());
}

TEST_F(AircoMqttBridgePublishTest, PublishesTheStatusRetained)
{
    EXPECT_CALL(*client, publish("airco/status", "HEATING", true)).Times(1);

    std::string status = "HEATING";
    bridge.onStatusChange(status);
}

TEST_F(AircoMqttBridgePublishTest, PublishesTheTargetTemperatureRetained)
{
    EXPECT_CALL(*client, publish("airco/target_temperature", "22", true)).Times(1);

    bridge.onTargetTemperatureChange(22);
}

TEST_F(AircoMqttBridgePublishTest, PublishesTheCurrentTemperatureRetained)
{
    EXPECT_CALL(*client, publish("airco/current_temperature", "-4", true)).Times(1);

    bridge.onCurrentTemperatureChange(-4);
}

// ---------------------------------------------------------------------------
// Receiving sensor readings (with the in-memory broker)
// ---------------------------------------------------------------------------

namespace {

class AircoMqttBridgeReceiveTest : public ::testing::Test
{
protected:
    bool queueIsEmpty()
    {
        queue->unblock(); // don't wait the full receive timeout
        return !queue->receive().has_value();
    }

    FakeMqttBroker broker;
    std::shared_ptr<MessageQueue<int>> queue = std::make_shared<MessageQueue<int>>();
    AircoMqttBridge bridge{broker.createClient(), queue, "house/airco"};
};

} // namespace

TEST_F(AircoMqttBridgeReceiveTest, ForwardsReadingsToTheTemperatureQueue)
{
    broker.publish("house/airco/sensor/temperature", "23");
    broker.publish("house/airco/sensor/temperature", "21.6");

    EXPECT_EQ(23, receiveWithin(*queue, 1s));
    EXPECT_EQ(22, receiveWithin(*queue, 1s));
}

TEST_F(AircoMqttBridgeReceiveTest, DropsInvalidReadings)
{
    broker.publish("house/airco/sensor/temperature", "warm");

    EXPECT_TRUE(queueIsEmpty());
}

TEST_F(AircoMqttBridgeReceiveTest, IgnoresOtherTopics)
{
    broker.publish("house/airco/status", "21");
    broker.publish("other/sensor/temperature", "21");

    EXPECT_TRUE(queueIsEmpty());
}

TEST_F(AircoMqttBridgeReceiveTest, PublishedValuesAreRetainedOnTheBroker)
{
    std::string status = "COOLING";
    bridge.onStatusChange(status);
    bridge.onTargetTemperatureChange(19);

    EXPECT_EQ(std::string("COOLING"), broker.retainedMessage("house/airco/status"));
    EXPECT_EQ(std::string("19"), broker.retainedMessage("house/airco/target_temperature"));
}
