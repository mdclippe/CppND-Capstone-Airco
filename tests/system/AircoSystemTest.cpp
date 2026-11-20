// System tests: the complete application, wired exactly like main.cpp and with
// the production timing (the simulated sensor changes one degree per second),
// driven through user scenarios. A RecordingObserver plays the role of the
// display (the Qt Monitor), so these tests run without a GUI.
#include <gmock/gmock.h>

#include <chrono>
#include <memory>
#include <string>
#include <thread>

#include "Airco.h"
#include "MessageQueue.h"
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

// A few degrees at one degree per second, with some margin
constexpr auto kScenarioTimeout = 10s;

class AircoSystemTest : public ::testing::Test
{
protected:
    AircoSystemTest()
    {
        // same wiring as main.cpp
        airco->addObserver(display);
        airco->addObserver(node1);
        // what the MainWindow constructor does at start-up (slider starts at 20)
        airco->setTargetTemperature(20);
        node1->setValue(20);
    }

    void TearDown() override { node1->stopSimulate(); }

    // --- user actions -----------------------------------------------------
    void pressPower() { airco->togglePower(); }
    void startSimulation() { node1->startSimulate(); }
    void setTarget(int temperature) { airco->setTargetTemperature(temperature); }
    void pressMode(const std::string& event, AircoModes expected)
    {
        airco->handleEvent(event);
        ASSERT_TRUE(waitUntil([&] { return airco->getMode() == expected; }));
    }

    // --- what the user sees -----------------------------------------------
    bool displayShowsTemperature(int temperature)
    {
        return waitUntil([&] { return display->lastCurrentTemperature() == temperature; },
                         kScenarioTimeout);
    }
    bool displayShows(const std::string& status, int temperature)
    {
        return waitUntil([&] {
            return display->lastStatus() == status && display->lastCurrentTemperature() == temperature;
        }, kScenarioTimeout);
    }
    bool displayShowsStatus(const std::string& status)
    {
        return waitUntil([&] { return display->lastStatus() == status; }, kScenarioTimeout);
    }

    std::shared_ptr<MessageQueue<int>> msgQueue = std::make_shared<MessageQueue<int>>(kTemperatureQueueCapacity);
    std::shared_ptr<TemperatureNode> node1 = std::make_shared<TemperatureNode>(msgQueue, 1);
    std::shared_ptr<Airco> airco = std::make_shared<Airco>(msgQueue);
    std::shared_ptr<RecordingObserver> display = std::make_shared<RecordingObserver>();
};

} // namespace

TEST_F(AircoSystemTest, UserWarmsUpTheRoomInAutoMode)
{
    pressPower();
    startSimulation();
    ASSERT_TRUE(displayShowsTemperature(20));

    setTarget(22);

    ASSERT_TRUE(displayShowsStatus("HEATING"));
    ASSERT_TRUE(displayShows("STANDBY", 22));
    EXPECT_THAT(display->statuses(), ElementsAre("STANDBY", "HEATING", "STANDBY"));
    EXPECT_THAT(withoutRepeats(display->currentTemperatures()), ElementsAre(20, 21, 22));
}

TEST_F(AircoSystemTest, UserCoolsDownTheRoomInAutoMode)
{
    pressPower();
    startSimulation();
    ASSERT_TRUE(displayShowsTemperature(20));

    setTarget(18);

    ASSERT_TRUE(displayShowsStatus("COOLING"));
    ASSERT_TRUE(displayShows("STANDBY", 18));
    EXPECT_THAT(display->statuses(), ElementsAre("STANDBY", "COOLING", "STANDBY"));
    EXPECT_THAT(withoutRepeats(display->currentTemperatures()), ElementsAre(20, 19, 18));
}

TEST_F(AircoSystemTest, InHeatModeTheAircoOnlyHeats)
{
    pressMode("heating_requested", AircoModes::MANUAL_HEAT);
    pressPower();
    startSimulation();
    ASSERT_TRUE(displayShowsTemperature(20));

    setTarget(19); // too warm: heat mode must not cool
    ASSERT_TRUE(displayShows("STANDBY", 19));
    EXPECT_THAT(display->statuses(), Not(Contains("COOLING")));

    // too cold: heat mode heats. Two degrees, because the next reading already
    // moves one degree towards the target and with one degree the airco would
    // never see the room as too cold.
    setTarget(21);
    ASSERT_TRUE(displayShowsStatus("HEATING"));
    ASSERT_TRUE(displayShows("STANDBY", 21));
    EXPECT_THAT(display->statuses(), ElementsAre("STANDBY", "HEATING", "STANDBY"));
}

TEST_F(AircoSystemTest, InCoolModeTheAircoOnlyCools)
{
    pressMode("cooling_requested", AircoModes::MANUAL_COOL);
    pressPower();
    startSimulation();
    ASSERT_TRUE(displayShowsTemperature(20));

    setTarget(21); // too cold: cool mode must not heat
    ASSERT_TRUE(displayShows("STANDBY", 21));
    EXPECT_THAT(display->statuses(), Not(Contains("HEATING")));

    setTarget(19); // too warm: cool mode cools (two degrees, see InHeatModeTheAircoOnlyHeats)
    ASSERT_TRUE(displayShowsStatus("COOLING"));
    ASSERT_TRUE(displayShows("STANDBY", 19));
    EXPECT_THAT(display->statuses(), ElementsAre("STANDBY", "COOLING", "STANDBY"));
}

TEST_F(AircoSystemTest, SwitchingOffStopsTheAircoWhileTheRoomKeepsChanging)
{
    pressPower();
    startSimulation();
    setTarget(25);
    ASSERT_TRUE(displayShowsStatus("HEATING"));

    pressPower();
    const auto readingsAtPowerOff = display->currentTemperatures().size();
    const int roomAtPowerOff = node1->getValue();
    std::this_thread::sleep_for(1500ms);

    EXPECT_EQ(AircoStates::OFF, airco->getCurrentState());
    EXPECT_EQ(std::string("OFF"), display->lastStatus());
    EXPECT_EQ(readingsAtPowerOff, display->currentTemperatures().size())
        << "a switched-off airco must not handle readings";
    EXPECT_GT(node1->getValue(), roomAtPowerOff) << "the simulated room keeps warming up";
}

TEST(AircoApplicationShutdownTest, ShutsDownCleanlyWhileEverythingIsRunning)
{
    const auto start = std::chrono::steady_clock::now();
    {
        // same objects and declaration order as main.cpp
        auto msgQueue = std::make_shared<MessageQueue<int>>(kTemperatureQueueCapacity);
        auto node1 = std::make_shared<TemperatureNode>(msgQueue, 1);
        auto airco = std::make_shared<Airco>(msgQueue);
        auto display = std::make_shared<RecordingObserver>();
        airco->addObserver(display);
        airco->addObserver(node1);

        airco->setTargetTemperature(25);
        node1->setValue(20);
        airco->togglePower();
        node1->startSimulate();
        airco->handleEvent("heating_requested");
        ASSERT_TRUE(waitUntil([&] { return display->lastStatus() == std::string("HEATING"); }));
    } // everything is destroyed while all threads are busy

    // stopping the simulation can take up to one simulation step (1 s)
    EXPECT_LT(std::chrono::steady_clock::now() - start, 5s);
}
