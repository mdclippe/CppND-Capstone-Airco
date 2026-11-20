// Unit tests for the abstract Node base class (through a minimal fake)
#include <gtest/gtest.h>

#include <memory>
#include <type_traits>
#include <utility>

#include "AircoObserver.h"
#include "Node.h"

static_assert(std::has_virtual_destructor_v<Node>, "Node is used as a polymorphic base class");
static_assert(std::has_virtual_destructor_v<AircoObserver>, "AircoObserver is used as a polymorphic base class");

namespace {

// Remembers that its destructor ran
class TrackedNode : public Node
{
public:
    TrackedNode(std::shared_ptr<MessageQueue<int>> queue, bool& destroyed)
        : Node(std::move(queue), 1), _destroyed(destroyed) {}
    ~TrackedNode() override { _destroyed = true; }
    void startSimulate() override {}
    void stopSimulate() override {}

private:
    bool& _destroyed;
};

class FakeNode : public Node
{
public:
    using Node::Node;
    void startSimulate() override { ++startCount; }
    void stopSimulate() override { ++stopCount; }

    int startCount = 0;
    int stopCount = 0;
};

class NodeTest : public ::testing::Test
{
protected:
    std::shared_ptr<MessageQueue<int>> queue = std::make_shared<MessageQueue<int>>();
    FakeNode node{queue, 7};
};

} // namespace

TEST_F(NodeTest, ConstructorStoresTheId)
{
    EXPECT_EQ(7, node.getID());
}

TEST_F(NodeTest, ValueStartsAtZero)
{
    EXPECT_EQ(0, node.getValue());
}

TEST_F(NodeTest, ValueCanBeChanged)
{
    node.setValue(21);
    EXPECT_EQ(21, node.getValue());

    node.setValue(-5);
    EXPECT_EQ(-5, node.getValue());
}

TEST_F(NodeTest, NameIsEmptyUntilSet)
{
    EXPECT_EQ("", node.getName());

    node.setName("living room");

    EXPECT_EQ("living room", node.getName());
}

TEST_F(NodeTest, SimulationIsDispatchedToTheConcreteNode)
{
    Node& base = node;

    base.startSimulate();
    base.stopSimulate();

    EXPECT_EQ(1, node.startCount);
    EXPECT_EQ(1, node.stopCount);
}

TEST(NodeLifetimeTest, DeletingThroughTheBaseClassRunsTheDerivedDestructor)
{
    bool destroyed = false;
    std::unique_ptr<Node> node =
        std::make_unique<TrackedNode>(std::make_shared<MessageQueue<int>>(), destroyed);

    node.reset();

    EXPECT_TRUE(destroyed);
}
