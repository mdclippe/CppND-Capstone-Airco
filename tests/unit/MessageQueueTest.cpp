// Unit tests for MessageQueue<T>
#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <future>
#include <string>
#include <thread>
#include <vector>

#include "MessageQueue.h"

using namespace std::chrono_literals;

TEST(MessageQueueTest, ReceiveReturnsTheSentMessage)
{
    MessageQueue<int> queue;
    queue.send(42);

    auto message = queue.receive();

    ASSERT_TRUE(message.has_value());
    EXPECT_EQ(42, *message);
}

TEST(MessageQueueTest, MessagesAreReceivedInFifoOrder)
{
    MessageQueue<int> queue;
    queue.send(1);
    queue.send(2);
    queue.send(3);

    EXPECT_EQ(1, queue.receive());
    EXPECT_EQ(2, queue.receive());
    EXPECT_EQ(3, queue.receive());
}

TEST(MessageQueueTest, WorksWithStringMessages)
{
    MessageQueue<std::string> queue;
    queue.send("cooling_requested");

    EXPECT_EQ(std::string("cooling_requested"), queue.receive());
}

TEST(MessageQueueTest, ReceiveOnEmptyQueueTimesOutWithNullopt)
{
    MessageQueue<int> queue;

    const auto start = std::chrono::steady_clock::now();
    auto message = queue.receive();
    const auto elapsed = std::chrono::steady_clock::now() - start;

    EXPECT_FALSE(message.has_value());
    EXPECT_GE(elapsed, 900ms) << "receive() is expected to wait ~1 s for a message";
    EXPECT_LT(elapsed, 3s);
}

TEST(MessageQueueTest, ClearDiscardsPendingMessages)
{
    MessageQueue<int> queue;
    queue.send(1);
    queue.send(2);

    queue.clear();
    queue.send(3);

    EXPECT_EQ(3, queue.receive());
}

TEST(MessageQueueTest, UnblockReleasesAWaitingReceiverImmediately)
{
    MessageQueue<int> queue;
    auto pending = std::async(std::launch::async, [&queue] { return queue.receive(); });
    std::this_thread::sleep_for(50ms); // give the receiver time to start waiting

    queue.unblock();

    ASSERT_EQ(std::future_status::ready, pending.wait_for(500ms))
        << "receiver still blocked after unblock()";
    EXPECT_FALSE(pending.get().has_value());
}

TEST(MessageQueueTest, UnblockStillDeliversQueuedMessages)
{
    MessageQueue<int> queue;
    queue.send(7);

    queue.unblock();

    EXPECT_EQ(7, queue.receive());
    EXPECT_FALSE(queue.receive().has_value());
}

TEST(MessageQueueTest, WaitingReceiverWakesUpWhenAnotherThreadSends)
{
    MessageQueue<int> queue;
    auto pending = std::async(std::launch::async, [&queue] { return queue.receive(); });
    std::this_thread::sleep_for(50ms);

    queue.send(5);

    ASSERT_EQ(std::future_status::ready, pending.wait_for(500ms));
    EXPECT_EQ(5, pending.get());
}

TEST(MessageQueueTest, ConcurrentProducersLoseNoMessagesAndKeepTheirOrder)
{
    static constexpr int kProducers = 4;
    static constexpr int kMessagesPerProducer = 250;
    MessageQueue<int> queue;

    std::vector<std::thread> producers;
    for (int p = 0; p < kProducers; ++p)
    {
        producers.emplace_back([&queue, p] {
            for (int i = 0; i < kMessagesPerProducer; ++i)
            {
                queue.send(p * kMessagesPerProducer + i);
            }
        });
    }

    std::vector<int> received;
    for (int i = 0; i < kProducers * kMessagesPerProducer; ++i)
    {
        auto message = queue.receive();
        if (!message.has_value())
        {
            break; // a message got lost, checked below
        }
        received.push_back(*message);
    }
    for (auto& producer : producers)
    {
        producer.join();
    }

    ASSERT_EQ(static_cast<size_t>(kProducers * kMessagesPerProducer), received.size());

    // messages of one producer arrive in the order they were sent
    std::vector<int> lastSeen(kProducers, -1);
    for (int value : received)
    {
        const int producer = value / kMessagesPerProducer;
        EXPECT_LT(lastSeen[producer], value);
        lastSeen[producer] = value;
    }

    // and every message arrived exactly once
    std::sort(received.begin(), received.end());
    for (int i = 0; i < kProducers * kMessagesPerProducer; ++i)
    {
        ASSERT_EQ(i, received[i]);
    }
}
