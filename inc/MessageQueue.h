#ifndef MESSAGE_QUEUE_H
#define MESSAGE_QUEUE_H
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <optional>

template <class T>
class MessageQueue 
{
public:
    std::optional<T> receive();
    void send(T&& msg);
    void clear();
    void unblock();

private:
    std::condition_variable _condition;
    std::mutex _mutex;
    std::queue<T> _queue;
    std::atomic<bool> _unblock = false;
};
template class MessageQueue<int>;
template class MessageQueue<std::string>;
#endif