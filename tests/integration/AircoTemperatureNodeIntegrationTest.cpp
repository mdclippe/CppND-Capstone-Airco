// Integration tests: Airco + TemperatureNode + MessageQueue working together,
// wired like main.cpp but with a fast simulation step.
// The simulated room follows the airco: it warms up while HEATING, cools down
// while COOLING and keeps its temperature in STANDBY / OFF.
#include <gmock/gmock.h>

#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "Airco.h"
#include "TemperatureNode.h"
#include "TestHelpers.h"

using namespace std::chrono_literals;
using airco_test::RecordingObserver;
using airco_test::waitUntil;
using airco_test::withoutRepeats;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::Not;

namespace {

// Slower than in the unit tests: the airco must have handled a reading (and
// told the node to stop heating) before the node takes its next step.
constexpr auto kStep = 50ms;

class AircoTemperatureNodeIntegrationTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        airco.addObserver(recorder);
        airco.addObserver(node);   // the room follows the airco status, like in main.cpp
        airco.setTargetTemperature(20);
        node->setValue(20);
    }

    void TearDown() override { node->stopSimulate(); }

    // Waits until the airco is in `state` and the last reading was `temperature`
    bool waitForStateAt(AircoStates state, int temperature)
    {
        return waitUntil([&] {
            return airco.getCurrentState() == state && recorder->lastCurrentTemperature() == temperature;
        });
    }

    bool waitForReadings(size_t count)
    {
        return waitUntil([&] { return recorder->currentTemperatures().size() >= count; });
    }

    bool switchMode(const std::string& event, AircoModes expected)
    {
        airco.handleEvent(event);
        return waitUntil([&] { return airco.getMode() == expected; });
    }

    // true when the simulated room temperature no longer changes
    bool roomTemperatureIsStable()
    {
        std::this_thread::sleep_for(2 * kStep); // a step that already started may still finish
        const int settled = node->getValue();
        std::this_thread::sleep_for(5 * kStep);
        return node->getValue() == settled;
    }

    std::shared_ptr<MessageQueue<int>> queue = std::make_shared<MessageQueue<int>>();
    std::shared_ptr<TemperatureNode> node = std::make_shared<TemperatureNode>(queue, 1, kStep);
    std::shared_ptr<RecordingObserver> recorder = std::make_shared<RecordingObserver>();
    Airco airco{queue};
};

} // namespace

TEST_F(AircoTemperatureNodeIntegrationTest, RoomDoesNotChangeWhileTheAircoIsOff)
{
    node->startSimulate();

    airco.setTargetTemperature(23);

    EXPECT_TRUE(roomTemperatureIsStable());
    EXPECT_EQ(20, node->getValue());
}

TEST_F(AircoTemperatureNodeIntegrationTest, AutoModeHeatsUntilTheRoomReachesTheTarget)
{
    airco.togglePower();
    airco.setTargetTemperature(23);

    node->startSimulate();

    ASSERT_TRUE(waitForStateAt(AircoStates::STANDBY, 23));
    EXPECT_THAT(recorder->statuses(), ElementsAre("STANDBY", "HEATING", "STANDBY"));
    EXPECT_THAT(withoutRepeats(recorder->currentTemperatures()), ElementsAre(20, 21, 22, 23));
    EXPECT_TRUE(roomTemperatureIsStable());
}

TEST_F(AircoTemperatureNodeIntegrationTest, AutoModeCoolsUntilTheRoomReachesTheTarget)
{
    airco.togglePower();
    airco.setTargetTemperature(18);

    node->startSimulate();

    ASSERT_TRUE(waitForStateAt(AircoStates::STANDBY, 18));
    EXPECT_THAT(recorder->statuses(), ElementsAre("STANDBY", "COOLING", "STANDBY"));
    EXPECT_THAT(withoutRepeats(recorder->currentTemperatures()), ElementsAre(20, 19, 18));
    EXPECT_TRUE(roomTemperatureIsStable());
}

TEST_F(AircoTemperatureNodeIntegrationTest, ManualHeatModeDoesNotCoolAWarmRoom)
{
    ASSERT_TRUE(switchMode("heating_requested", AircoModes::MANUAL_HEAT));
    airco.togglePower();
    airco.setTargetTemperature(18); // below the room temperature

    node->startSimulate();

    ASSERT_TRUE(waitForReadings(5));
    EXPECT_THAT(withoutRepeats(recorder->currentTemperatures()), ElementsAre(20));
    EXPECT_THAT(recorder->statuses(), ElementsAre("STANDBY"));
}

TEST_F(AircoTemperatureNodeIntegrationTest, ManualHeatModeHeatsAColdRoom)
{
    ASSERT_TRUE(switchMode("heating_requested", AircoModes::MANUAL_HEAT));
    airco.togglePower();
    airco.setTargetTemperature(22);

    node->startSimulate();

    ASSERT_TRUE(waitForStateAt(AircoStates::STANDBY, 22));
    EXPECT_THAT(recorder->statuses(), ElementsAre("STANDBY", "HEATING", "STANDBY"));
}

TEST_F(AircoTemperatureNodeIntegrationTest, ManualCoolModeDoesNotHeatAColdRoom)
{
    ASSERT_TRUE(switchMode("cooling_requested", AircoModes::MANUAL_COOL));
    airco.togglePower();
    airco.setTargetTemperature(22); // above the room temperature

    node->startSimulate();

    ASSERT_TRUE(waitForReadings(5));
    EXPECT_THAT(withoutRepeats(recorder->currentTemperatures()), ElementsAre(20));
    EXPECT_THAT(recorder->statuses(), ElementsAre("STANDBY"));
}

TEST_F(AircoTemperatureNodeIntegrationTest, ManualCoolModeCoolsAWarmRoom)
{
    ASSERT_TRUE(switchMode("cooling_requested", AircoModes::MANUAL_COOL));
    airco.togglePower();
    airco.setTargetTemperature(18);

    node->startSimulate();

    ASSERT_TRUE(waitForStateAt(AircoStates::STANDBY, 18));
    EXPECT_THAT(recorder->statuses(), ElementsAre("STANDBY", "COOLING", "STANDBY"));
}

TEST_F(AircoTemperatureNodeIntegrationTest, SwitchingToCoolModeWhileHeatingStopsWarmingTheRoom)
{
    airco.togglePower();
    airco.setTargetTemperature(35); // far away, the room keeps warming during the test
    node->startSimulate();
    ASSERT_TRUE(waitUntil([&] { return airco.getCurrentState() == AircoStates::HEATING; }));

    ASSERT_TRUE(switchMode("cooling_requested", AircoModes::MANUAL_COOL));

    EXPECT_TRUE(waitUntil([&] { return airco.getCurrentState() == AircoStates::STANDBY; }));
    EXPECT_TRUE(roomTemperatureIsStable());
    EXPECT_THAT(recorder->statuses(), Not(Contains("COOLING")));
}

TEST_F(AircoTemperatureNodeIntegrationTest, ReadingsAreOnlyHandledWhileTheAircoIsOn)
{
    node->startSimulate();
    std::this_thread::sleep_for(5 * kStep);

    EXPECT_EQ(AircoStates::OFF, airco.getCurrentState());
    EXPECT_TRUE(recorder->currentTemperatures().empty());

    airco.togglePower();

    EXPECT_TRUE(waitUntil([&] { return recorder->lastCurrentTemperature() == 20; }));
    EXPECT_EQ(AircoStates::STANDBY, airco.getCurrentState());
}

TEST_F(AircoTemperatureNodeIntegrationTest, PowerOffWhileHeatingStopsTheAircoAndTheRoomWarming)
{
    airco.togglePower();
    airco.setTargetTemperature(35);
    node->startSimulate();
    ASSERT_TRUE(waitUntil([&] { return airco.getCurrentState() == AircoStates::HEATING; }));

    airco.togglePower();
    const auto readingsAtPowerOff = recorder->currentTemperatures().size();

    EXPECT_TRUE(roomTemperatureIsStable());
    EXPECT_EQ(AircoStates::OFF, airco.getCurrentState());
    EXPECT_EQ(std::string("OFF"), recorder->lastStatus());
    EXPECT_EQ(readingsAtPowerOff, recorder->currentTemperatures().size())
        << "a switched-off airco must not handle readings";
}

// Wired like main.cpp: a bounded temperature queue between node and airco
TEST(AircoTemperatureQueueIntegrationTest, ReadingsDoNotPileUpWhileTheAircoIsOff)
{
    auto queue = std::make_shared<MessageQueue<int>>(kTemperatureQueueCapacity);
    auto node = std::make_shared<TemperatureNode>(queue, 1, 2ms);
    Airco airco{queue}; // switched off: nobody reads the queue
    airco.addObserver(node);
    node->setValue(20);

    node->startSimulate();
    // far more readings than the capacity, even with a coarse (~15 ms) sleep timer
    std::this_thread::sleep_for(500ms);
    node->stopSimulate();

    std::vector<int> waiting;
    queue->unblock(); // the last receive returns at once when the queue is empty
    while (auto reading = queue->receive())
    {
        waiting.push_back(*reading);
    }
    EXPECT_EQ(kTemperatureQueueCapacity, waiting.size());
}
