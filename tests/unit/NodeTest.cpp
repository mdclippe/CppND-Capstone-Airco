// Unit tests for the abstract Node base class (through a minimal fake)
#include <gtest/gtest.h>

#include <memory>

#include "Node.h"

namespace {

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
