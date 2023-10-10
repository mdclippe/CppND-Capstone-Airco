#include "MessageQueue.h"
#include <iostream>
template <typename T>
std::optional<T> MessageQueue<T>::receive()
{
    std::unique_lock<std::mutex> uLock(_mutex);
    auto timeout = std::chrono::seconds(1);
    _condition.wait_for(uLock,timeout, [this]{return !_queue.empty() || _unblock.load();});
    
    if (!_queue.empty())
    {
        auto msg = std::move(_queue.front());
        _queue.pop();
        return msg;
    }
    else
    {
        return std::nullopt;
    }
}


template <typename T>
void MessageQueue<T>::send(T&& msg)
{
    std::lock_guard<std::mutex> lg(_mutex);
    _queue.push(msg);
    _condition.notify_all();
}

template <typename T>
void MessageQueue<T>::clear()
{
    std::lock_guard<std::mutex> lg(_mutex);
    std::queue<T> emptyQueue;
    std::swap(_queue, emptyQueue);
}

template <typename T>
void MessageQueue<T>::unblock()
{
    std::lock_guard<std::mutex> lg(_mutex);
    _unblock = true;
    _condition.notify_all();
}
