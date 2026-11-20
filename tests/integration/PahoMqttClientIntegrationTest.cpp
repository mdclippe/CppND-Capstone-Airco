// Integration tests for PahoMqttClient (Eclipse Paho).
// The tests in PahoMqttClientBrokerTest need a real broker and are skipped without one.
#include <gmock/gmock.h>

#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "MqttBrokerTestSupport.h"
#include "PahoMqttClient.h"
#include "TestHelpers.h"

using namespace std::chrono_literals;
using airco_test::testBrokerUri;
using airco_test::uniqueName;
using airco_test::waitUntil;
using ::testing::Contains;
using ::testing::ElementsAre;

namespace {

// a port nobody listens on
const std::string kNoBroker = "tcp://127.0.0.1:1";

// Remembers the messages a client receives
class Inbox
{
public:
    MqttClient::MessageHandler handler()
    {
        return [this](const std::string&, const std::string& payload) {
            std::lock_guard<std::mutex> lock(_mutex);
            _payloads.push_back(payload);
        };
    }
    std::vector<std::string> payloads() const
    {
        std::lock_guard<std::mutex> lock(_mutex);
        return _payloads;
    }

private:
    mutable std::mutex _mutex;
    std::vector<std::string> _payloads;
};

} // namespace

// ---------------------------------------------------------------------------
// Without a broker
// ---------------------------------------------------------------------------

TEST(PahoMqttClientTest, ConnectReportsAnUnreachableBroker)
{
    PahoMqttClient client(kNoBroker, uniqueName("airco-test"));

    const auto start = std::chrono::steady_clock::now();
    EXPECT_FALSE(client.connect(2s));

    EXPECT_LT(std::chrono::steady_clock::now() - start, 10s);
    EXPECT_FALSE(client.isConnected());
}

TEST(PahoMqttClientTest, PublishAndSubscribeWhileDisconnectedAreHarmless)
{
    PahoMqttClient client(kNoBroker, uniqueName("airco-test"));

    EXPECT_NO_THROW(client.publish("airco-test/topic", "21", true));
    EXPECT_NO_THROW(client.subscribe("airco-test/topic", [](const std::string&, const std::string&) {}));
}

// ---------------------------------------------------------------------------
// With a broker (skipped when none is reachable)
// ---------------------------------------------------------------------------

namespace {

class PahoMqttClientBrokerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        SKIP_WITHOUT_MQTT_BROKER();
        publisher = std::make_unique<PahoMqttClient>(testBrokerUri(), uniqueName("airco-test-pub"));
        subscriber = std::make_unique<PahoMqttClient>(testBrokerUri(), uniqueName("airco-test-sub"));
        ASSERT_TRUE(publisher->connect());
    }

    void TearDown() override
    {
        if (publisher && publisher->isConnected())
        {
            publisher->publish(topic, "", true); // an empty retained message removes it from the broker
        }
    }

    const std::string topic = uniqueName("airco-test") + "/value";
    std::unique_ptr<PahoMqttClient> publisher;
    std::unique_ptr<PahoMqttClient> subscriber;
    Inbox inbox;
};

} // namespace

TEST_F(PahoMqttClientBrokerTest, SubscriberReceivesPublishedMessages)
{
    ASSERT_TRUE(subscriber->connect());
    subscriber->subscribe(topic, inbox.handler());

    // the subscription becomes active asynchronously: publish until it arrives
    EXPECT_TRUE(waitUntil([&] {
        publisher->publish(topic, "21", false);
        return !inbox.payloads().empty();
    }, 5s, 100ms));
    EXPECT_THAT(inbox.payloads(), Contains("21"));
}

TEST_F(PahoMqttClientBrokerTest, RetainedMessageReachesALateSubscriber)
{
    publisher->publish(topic, "HEATING", true);

    ASSERT_TRUE(subscriber->connect());
    subscriber->subscribe(topic, inbox.handler());

    ASSERT_TRUE(waitUntil([&] { return !inbox.payloads().empty(); }));
    EXPECT_THAT(inbox.payloads(), ElementsAre("HEATING"));
}

TEST_F(PahoMqttClientBrokerTest, SubscriptionsMadeBeforeConnectingAreActivated)
{
    publisher->publish(topic, "STANDBY", true);
    subscriber->subscribe(topic, inbox.handler()); // not connected yet

    ASSERT_TRUE(subscriber->connect());

    EXPECT_TRUE(waitUntil([&] { return !inbox.payloads().empty(); }));
}
