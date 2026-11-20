// Unit tests for TemperatureNode (simulated temperature sensor)
#include <gmock/gmock.h>

#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "TemperatureNode.h"
#include "TestHelpers.h"

using namespace std::chrono_literals;
using airco_test::receiveWithin;
using airco_test::waitUntil;
using ::testing::ElementsAre;

namespace {

// Fast simulation step so the tests do not have to wait a second per degree
constexpr auto kStep = 10ms;

class TemperatureNodeTest : public ::testing::Test
{
protected:
    void TearDown() override { node.stopSimulate(); }

    // the airco notifies its observers (like the node) of every status change
    void aircoReports(const char* status)
    {
        std::string text = status;
        node.onStatusChange(text);
    }

    // Waits for `count` readings sent by the node to the queue
    std::vector<int> readings(size_t count)
    {
        std::vector<int> result;
        while (result.size() < count)
        {
            auto reading = receiveWithin(*queue, 2s);
            if (!reading.has_value())
            {
                ADD_FAILURE() << "no reading received after " << result.size() << " readings";
                break;
            }
            result.push_back(*reading);
        }
        return result;
    }

    std::shared_ptr<MessageQueue<int>> queue = std::make_shared<MessageQueue<int>>();
    TemperatureNode node{queue, 1, kStep};
};

} // namespace

TEST_F(TemperatureNodeTest, KeepsTheIdItWasCreatedWith)
{
    EXPECT_EQ(1, node.getID());
}

TEST_F(TemperatureNodeTest, SendsNothingBeforeSimulationStarts)
{
    std::this_thread::sleep_for(5 * kStep);
    queue->unblock(); // don't wait the full receive timeout

    EXPECT_FALSE(queue->receive().has_value());
}

TEST_F(TemperatureNodeTest, RoomStaysTheSameAsLongAsTheAircoDoesNothing)
{
    node.setValue(20);

    node.startSimulate();

    EXPECT_THAT(readings(3), ElementsAre(20, 20, 20));
}

TEST_F(TemperatureNodeTest, WarmsUpOneDegreePerStepWhileTheAircoHeats)
{
    node.setValue(20);
    aircoReports("HEATING");

    node.startSimulate();

    EXPECT_THAT(readings(3), ElementsAre(21, 22, 23));
}

TEST_F(TemperatureNodeTest, CoolsDownOneDegreePerStepWhileTheAircoCools)
{
    node.setValue(20);
    aircoReports("COOLING");

    node.startSimulate();

    EXPECT_THAT(readings(3), ElementsAre(19, 18, 17));
}

TEST_F(TemperatureNodeTest, TargetTemperatureAloneDoesNotChangeTheRoom)
{
    // e.g. HEAT mode with a target below the room temperature: the airco is in standby
    node.setValue(20);
    node.onTargetTemperatureChange(15);
    aircoReports("STANDBY");

    node.startSimulate();

    EXPECT_THAT(readings(3), ElementsAre(20, 20, 20));
}

TEST_F(TemperatureNodeTest, FollowsTheAircoWhileSimulating)
{
    node.setValue(20);
    node.startSimulate();

    aircoReports("HEATING");
    ASSERT_TRUE(waitUntil([&] { return node.getValue() >= 22; }));

    aircoReports("COOLING");
    ASSERT_TRUE(waitUntil([&] { return node.getValue() <= 19; }));

    aircoReports("STANDBY");
    std::this_thread::sleep_for(2 * kStep); // a step that already started may still finish
    const int settled = node.getValue();
    std::this_thread::sleep_for(5 * kStep);
    EXPECT_EQ(settled, node.getValue());
}

TEST_F(TemperatureNodeTest, StartSimulateDiscardsOldQueueContent)
{
    queue->send(99);
    node.setValue(20);

    node.startSimulate();

    EXPECT_EQ(20, receiveWithin(*queue));
}

TEST_F(TemperatureNodeTest, StopSimulateStopsTheReadings)
{
    node.setValue(20);
    node.startSimulate();
    ASSERT_TRUE(receiveWithin(*queue).has_value());

    node.stopSimulate();
    queue->clear();
    std::this_thread::sleep_for(5 * kStep);
    queue->unblock();

    EXPECT_FALSE(queue->receive().has_value());
}

TEST_F(TemperatureNodeTest, StopSimulateWithoutStartIsHarmless)
{
    node.stopSimulate();
    node.stopSimulate();
    SUCCEED();
}

TEST_F(TemperatureNodeTest, StartingTwiceKeepsASingleSimulation)
{
    node.setValue(20);
    aircoReports("HEATING");

    node.startSimulate();
    node.startSimulate(); // used to replace the running (joinable) thread -> std::terminate

    EXPECT_THAT(readings(4), ElementsAre(21, 22, 23, 24));
}

TEST_F(TemperatureNodeTest, CanBeRestartedAndContinuesFromItsLastValue)
{
    node.setValue(20);
    aircoReports("HEATING");
    node.startSimulate();
    EXPECT_THAT(readings(2), ElementsAre(21, 22));
    node.stopSimulate();
    const int lastValue = node.getValue();

    node.startSimulate();

    auto next = receiveWithin(*queue);
    ASSERT_TRUE(next.has_value());
    EXPECT_EQ(lastValue + 1, *next);
}

// STANDBY and OFF stop the temperature change
class TemperatureNodeIdleStatusTest : public TemperatureNodeTest,
                                      public ::testing::WithParamInterface<const char*> {};

TEST_P(TemperatureNodeIdleStatusTest, KeepsTheRoomTemperature)
{
    node.setValue(20);
    aircoReports("HEATING");
    aircoReports(GetParam());

    node.startSimulate();

    EXPECT_THAT(readings(3), ElementsAre(20, 20, 20));
}

INSTANTIATE_TEST_SUITE_P(AircoStatus, TemperatureNodeIdleStatusTest, ::testing::Values("STANDBY", "OFF"));

TEST(TemperatureNodeLifetimeTest, DestroyingASimulatingNodeStopsItsThread)
{
    auto queue = std::make_shared<MessageQueue<int>>();
    const auto start = std::chrono::steady_clock::now();
    {
        TemperatureNode node(queue, 1, kStep);
        node.startSimulate();
        ASSERT_TRUE(receiveWithin(*queue).has_value());
    } // destructor must stop and join the simulation thread
    EXPECT_LT(std::chrono::steady_clock::now() - start, 2s);

    queue->clear();
    std::this_thread::sleep_for(5 * kStep);
    queue->unblock();
    EXPECT_FALSE(queue->receive().has_value()) << "thread still sending after destruction";
}
