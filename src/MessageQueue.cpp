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
        // woken up by unblock() (or timed out) : the unblock request has been handled,
        // so the next receive() waits for messages again
        _unblock = false;
        return std::nullopt;
    }
}


template <typename T>
void MessageQueue<T>::send(T&& msg)
{
    std::lock_guard<std::mutex> lg(_mutex);
    if (_capacity > 0 && _queue.size() >= _capacity)
    {
        // full : drop the oldest message, the newest one is the most relevant
        _queue.pop();
    }
    _queue.push(std::move(msg));
    _condition.notify_all();
}

template <typename T>
void MessageQueue<T>::clear()
{
    std::lock_guard<std::mutex> lg(_mutex);
    std::queue<T> emptyQueue;
    std::swap(_queue, emptyQueue);
}


/**
 * @brief This is to make sure that there is no blocking wait in the queue. This is not really necessary
 * as the condition variable also has a timeout, but i think it can do no harm to have it.
 * The receive() that is waiting (or the next call to receive()) returns without a message.
 * That receive() handles the request, later calls wait for messages again.
 *
 * @tparam T 
 */
template <typename T>
void MessageQueue<T>::unblock()
{
    std::lock_guard<std::mutex> lg(_mutex);
    _unblock = true;
    _condition.notify_all();
}


// the message types used in this project
template class MessageQueue<int>;
template class MessageQueue<std::string>;
