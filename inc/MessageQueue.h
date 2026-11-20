#ifndef MESSAGE_QUEUE_H
#define MESSAGE_QUEUE_H
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <optional>
#include <string>
#include <cstddef>

template <class T>
class MessageQueue
{
public:
    // capacity 0 means unbounded. When a bounded queue is full, send() drops the oldest message.
    explicit MessageQueue(std::size_t capacity = 0) : _capacity(capacity) {}
    std::optional<T> receive();
    void send(T&& msg);
    void clear();
    void unblock();

private:
    std::condition_variable _condition;
    std::mutex _mutex;
    std::queue<T> _queue;
    std::size_t _capacity;
    std::atomic<bool> _unblock = false;
};
// explicitly instantiated in MessageQueue.cpp
extern template class MessageQueue<int>;
extern template class MessageQueue<std::string>;
#endif