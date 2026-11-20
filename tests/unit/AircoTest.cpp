// Unit tests for Airco.
// The test feeds the temperature queue itself (no TemperatureNode) and
// observes the airco through mock / recording observers.
#include <gmock/gmock.h>

#include <chrono>
#include <memory>
#include <ostream>
#include <string>
#include <thread>

#include "Airco.h"
#include "TestHelpers.h"

using namespace std::chrono_literals;
using airco_test::MockAircoObserver;
using airco_test::RecordingObserver;
using airco_test::waitUntil;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::StrictMock;

namespace {

class AircoTest : public ::testing::Test
{
protected:
    void SetUp() override { airco.addObserver(recorder); }

    // Sends a reading and waits until the airco has completely handled it.
    // A second identical reading is used as a fence: once that one is
    // reported, the first one has been through the state evaluation.
    bool feedReading(int temperature)
    {
        const auto alreadySeen = recorder->currentTemperatures().size();
        airco_test::sendCopy(*queue, temperature);
        airco_test::sendCopy(*queue, temperature);
        return waitUntil([&] { return recorder->currentTemperatures().size() >= alreadySeen + 2; });
    }

    // Sends a mode event and waits until the event thread handled it.
    bool switchMode(const std::string& event, AircoModes expected)
    {
        airco.handleEvent(event);
        return waitUntil([&] { return airco.getMode() == expected; });
    }

    std::shared_ptr<MessageQueue<int>> queue = std::make_shared<MessageQueue<int>>();
    std::shared_ptr<RecordingObserver> recorder = std::make_shared<RecordingObserver>();
    Airco airco{queue};
};

} // namespace

// ---------------------------------------------------------------------------
// Initial state and target temperature
// ---------------------------------------------------------------------------

TEST_F(AircoTest, StartsSwitchedOffInAutoModeWithTargetOf20Degrees)
{
    EXPECT_EQ(AircoStates::OFF, airco.getCurrentState());
    EXPECT_EQ(AircoModes::AUTO, airco.getMode());
    EXPECT_EQ(20, airco.getTargetTemperature());
}

TEST_F(AircoTest, SetTargetTemperatureStoresTheNewTarget)
{
    airco.setTargetTemperature(24);

    EXPECT_EQ(24, airco.getTargetTemperature());
}

// ---------------------------------------------------------------------------
// Observers
// ---------------------------------------------------------------------------

TEST_F(AircoTest, SetTargetTemperatureNotifiesEveryObserver)
{
    auto first = std::make_shared<StrictMock<MockAircoObserver>>();
    auto second = std::make_shared<StrictMock<MockAircoObserver>>();
    airco.addObserver(first);
    airco.addObserver(second);

    EXPECT_CALL(*first, onTargetTemperatureChange(24));
    EXPECT_CALL(*second, onTargetTemperatureChange(24));

    airco.setTargetTemperature(24);

    ::testing::Mock::VerifyAndClearExpectations(first.get());
    ::testing::Mock::VerifyAndClearExpectations(second.get());
}

TEST_F(AircoTest, RemovedObserverIsNoLongerNotified)
{
    // StrictMock: any call on it fails the test
    auto observer = std::make_shared<StrictMock<MockAircoObserver>>();
    airco.addObserver(observer);

    airco.removeObserver(observer);
    airco.setTargetTemperature(24);
    airco.setState(AircoStates::HEATING);
}

TEST_F(AircoTest, RemovingAnUnknownObserverChangesNothing)
{
    airco.removeObserver(std::make_shared<MockAircoObserver>());

    airco.setTargetTemperature(24);

    EXPECT_THAT(recorder->targetTemperatures(), ElementsAre(24));
}

TEST_F(AircoTest, SetStateReportsAStatusOnlyWhenTheStateChanges)
{
    auto observer = std::make_shared<StrictMock<MockAircoObserver>>();
    airco.addObserver(observer);
    EXPECT_CALL(*observer, onStatusChange(Eq(std::string("HEATING")))).Times(1);

    airco.setState(AircoStates::HEATING);
    airco.setState(AircoStates::HEATING);

    EXPECT_EQ(AircoStates::HEATING, airco.getCurrentState());
    ::testing::Mock::VerifyAndClearExpectations(observer.get());
}

// Every state is reported to the observers with its own name
namespace {
struct StatusCase
{
    AircoStates state;
    const char* status;
};
void PrintTo(const StatusCase& c, std::ostream* os) { *os << c.status; }

class AircoStatusNameTest : public AircoTest, public ::testing::WithParamInterface<StatusCase> {};
} // namespace

TEST_P(AircoStatusNameTest, ReportsTheStateByName)
{
    const StatusCase& param = GetParam();
    if (param.state == AircoStates::OFF)
    {
        airco.setState(AircoStates::STANDBY); // OFF is the initial state, leave it first
    }

    airco.setState(param.state);

    ASSERT_TRUE(recorder->lastStatus().has_value());
    EXPECT_EQ(param.status, *recorder->lastStatus());
}

INSTANTIATE_TEST_SUITE_P(AllStates, AircoStatusNameTest,
    ::testing::Values(StatusCase{AircoStates::OFF, "OFF"},
                      StatusCase{AircoStates::COOLING, "COOLING"},
                      StatusCase{AircoStates::HEATING, "HEATING"},
                      StatusCase{AircoStates::STANDBY, "STANDBY"}),
    [](const ::testing::TestParamInfo<StatusCase>& info) { return std::string(info.param.status); });

// ---------------------------------------------------------------------------
// Power
// ---------------------------------------------------------------------------

TEST_F(AircoTest, PowerOnReportsStandbyAndPowerOffReportsOff)
{
    auto observer = std::make_shared<StrictMock<MockAircoObserver>>();
    airco.addObserver(observer);
    {
        InSequence order;
        EXPECT_CALL(*observer, onStatusChange(Eq(std::string("STANDBY"))));
        EXPECT_CALL(*observer, onStatusChange(Eq(std::string("OFF"))));
    }

    airco.togglePower();
    EXPECT_EQ(AircoStates::STANDBY, airco.getCurrentState());

    airco.togglePower();
    EXPECT_EQ(AircoStates::OFF, airco.getCurrentState());

    ::testing::Mock::VerifyAndClearExpectations(observer.get());
}

TEST_F(AircoTest, IgnoresTemperatureReadingsWhileOff)
{
    queue->send(15);
    std::this_thread::sleep_for(100ms);

    EXPECT_EQ(AircoStates::OFF, airco.getCurrentState());
    EXPECT_TRUE(recorder->currentTemperatures().empty());
}

TEST_F(AircoTest, StopsEvaluatingTheTemperatureWhenSwitchedOff)
{
    airco.togglePower();
    ASSERT_TRUE(feedReading(15));
    ASSERT_EQ(AircoStates::HEATING, airco.getCurrentState());

    airco.togglePower();
    queue->send(25);
    std::this_thread::sleep_for(100ms);

    EXPECT_EQ(AircoStates::OFF, airco.getCurrentState());
    EXPECT_FALSE(recorder->hasSeenCurrentTemperature(25));
}

TEST_F(AircoTest, KeepsProcessingReadingsAfterAPowerCycle)
{
    airco.togglePower(); // on
    airco.togglePower(); // off
    airco.togglePower(); // on again

    ASSERT_TRUE(feedReading(15));

    EXPECT_EQ(AircoStates::HEATING, airco.getCurrentState());
}

// ---------------------------------------------------------------------------
// Temperature readings
// ---------------------------------------------------------------------------

TEST_F(AircoTest, ForwardsEveryTemperatureReadingToTheObservers)
{
    airco.togglePower();

    queue->send(18);
    queue->send(19);

    ASSERT_TRUE(waitUntil([&] { return recorder->currentTemperatures().size() >= 2; }));
    EXPECT_THAT(recorder->currentTemperatures(), ElementsAre(18, 19));
}

TEST_F(AircoTest, ReactsToANewTargetWithoutANewReading)
{
    airco.togglePower();
    ASSERT_TRUE(feedReading(20));
    ASSERT_EQ(AircoStates::STANDBY, airco.getCurrentState());

    airco.setTargetTemperature(23);

    // the reading thread re-evaluates at least once per second
    EXPECT_TRUE(waitUntil([&] { return airco.getCurrentState() == AircoStates::HEATING; }));
}

// AUTO mode: heat below the target, cool above it, standby when equal
namespace {
struct ReadingCase
{
    int reading;
    AircoStates expected;
};
void PrintTo(const ReadingCase& c, std::ostream* os)
{
    *os << "reading " << c.reading << " -> ";
    PrintTo(c.expected, os);
}

class AircoAutoModeTest : public AircoTest, public ::testing::WithParamInterface<ReadingCase> {};
} // namespace

TEST_P(AircoAutoModeTest, ChoosesTheStateFromReadingAndTarget)
{
    airco.setTargetTemperature(20);
    airco.togglePower();

    ASSERT_TRUE(feedReading(GetParam().reading));

    EXPECT_EQ(GetParam().expected, airco.getCurrentState());
}

INSTANTIATE_TEST_SUITE_P(TargetOf20Degrees, AircoAutoModeTest,
    ::testing::Values(ReadingCase{15, AircoStates::HEATING},
                      ReadingCase{19, AircoStates::HEATING},
                      ReadingCase{20, AircoStates::STANDBY},
                      ReadingCase{21, AircoStates::COOLING},
                      ReadingCase{25, AircoStates::COOLING}));

// ---------------------------------------------------------------------------
// Modes
// ---------------------------------------------------------------------------

TEST_F(AircoTest, ModeEventsSwitchTheMode)
{
    EXPECT_TRUE(switchMode("cooling_requested", AircoModes::MANUAL_COOL));
    EXPECT_TRUE(switchMode("heating_requested", AircoModes::MANUAL_HEAT));
    EXPECT_TRUE(switchMode("auto_requested", AircoModes::AUTO));
}

TEST_F(AircoTest, ModeEventsDoNotSwitchThePowerOn)
{
    ASSERT_TRUE(switchMode("heating_requested", AircoModes::MANUAL_HEAT));

    EXPECT_EQ(AircoStates::OFF, airco.getCurrentState());
}

TEST_F(AircoTest, UnknownEventsAreIgnored)
{
    ASSERT_TRUE(switchMode("cooling_requested", AircoModes::MANUAL_COOL));

    airco.handleEvent("turbo_requested");
    airco.handleEvent("");
    std::this_thread::sleep_for(50ms); // the event thread handles an event within a few ms

    EXPECT_EQ(AircoModes::MANUAL_COOL, airco.getMode());
}

// Manual modes only heat or only cool, otherwise they stay in standby
namespace {
struct ModeCase
{
    const char* event;
    AircoModes mode;
    int reading;
    AircoStates expected;
};
void PrintTo(const ModeCase& c, std::ostream* os)
{
    *os << c.event << ", reading " << c.reading << " -> ";
    PrintTo(c.expected, os);
}

class AircoModeBehaviourTest : public AircoTest, public ::testing::WithParamInterface<ModeCase> {};
} // namespace

TEST_P(AircoModeBehaviourTest, ChoosesTheStateAllowedByTheMode)
{
    const ModeCase& param = GetParam();
    airco.setTargetTemperature(20);
    airco.togglePower();
    ASSERT_TRUE(switchMode(param.event, param.mode));

    ASSERT_TRUE(feedReading(param.reading));

    EXPECT_EQ(param.expected, airco.getCurrentState());
}

INSTANTIATE_TEST_SUITE_P(TargetOf20Degrees, AircoModeBehaviourTest,
    ::testing::Values(
        ModeCase{"heating_requested", AircoModes::MANUAL_HEAT, 15, AircoStates::HEATING},
        ModeCase{"heating_requested", AircoModes::MANUAL_HEAT, 20, AircoStates::STANDBY},
        ModeCase{"heating_requested", AircoModes::MANUAL_HEAT, 25, AircoStates::STANDBY}, // never cools
        ModeCase{"cooling_requested", AircoModes::MANUAL_COOL, 25, AircoStates::COOLING},
        ModeCase{"cooling_requested", AircoModes::MANUAL_COOL, 20, AircoStates::STANDBY},
        ModeCase{"cooling_requested", AircoModes::MANUAL_COOL, 15, AircoStates::STANDBY}, // never heats
        ModeCase{"auto_requested", AircoModes::AUTO, 15, AircoStates::HEATING},
        ModeCase{"auto_requested", AircoModes::AUTO, 25, AircoStates::COOLING}));

// ---------------------------------------------------------------------------
// Lifetime
// ---------------------------------------------------------------------------

TEST(AircoLifetimeTest, DestroyingAnAircoThatWasNeverSwitchedOnIsQuick)
{
    auto queue = std::make_shared<MessageQueue<int>>();
    const auto start = std::chrono::steady_clock::now();

    {
        Airco airco(queue);
    }

    EXPECT_LT(std::chrono::steady_clock::now() - start, 500ms);
}

TEST(AircoLifetimeTest, DestroyingARunningAircoStopsItsThreads)
{
    auto queue = std::make_shared<MessageQueue<int>>();
    auto recorder = std::make_shared<RecordingObserver>();
    const auto start = std::chrono::steady_clock::now();

    {
        Airco airco(queue);
        airco.addObserver(recorder);
        airco.togglePower();
        for (int temperature = 10; temperature < 30; ++temperature)
        {
            airco_test::sendCopy(*queue, temperature);
        }
        airco.handleEvent("cooling_requested");
    } // destructor must join both threads while they are busy

    EXPECT_LT(std::chrono::steady_clock::now() - start, 2s);
}
