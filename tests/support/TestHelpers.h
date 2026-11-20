#ifndef AIRCO_TEST_HELPERS_H
#define AIRCO_TEST_HELPERS_H

#include <gmock/gmock.h>

#include <chrono>
#include <mutex>
#include <optional>
#include <ostream>
#include <string>
#include <thread>
#include <vector>

#include "Airco.h"
#include "AircoObserver.h"
#include "MessageQueue.h"

// Readable output for AircoStates / AircoModes in assertion failures
// (must live in the namespace of the enums for gtest to find it).
inline void PrintTo(AircoStates state, std::ostream* os)
{
    switch (state)
    {
        case AircoStates::OFF:     *os << "OFF"; break;
        case AircoStates::COOLING: *os << "COOLING"; break;
        case AircoStates::HEATING: *os << "HEATING"; break;
        case AircoStates::STANDBY: *os << "STANDBY"; break;
    }
}

inline void PrintTo(AircoModes mode, std::ostream* os)
{
    switch (mode)
    {
        case AircoModes::AUTO:        *os << "AUTO"; break;
        case AircoModes::MANUAL_HEAT: *os << "MANUAL_HEAT"; break;
        case AircoModes::MANUAL_COOL: *os << "MANUAL_COOL"; break;
    }
}

namespace airco_test {

using namespace std::chrono_literals;

// Default time a test waits for something asynchronous to happen.
constexpr std::chrono::milliseconds kAsyncTimeout = 5s;

// Polls `condition` until it is true or `timeout` expires.
template <class Condition>
bool waitUntil(Condition condition,
               std::chrono::milliseconds timeout = kAsyncTimeout,
               std::chrono::milliseconds pollInterval = 2ms)
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (condition())
        {
            return true;
        }
        std::this_thread::sleep_for(pollInterval);
    }
    return condition();
}

// MessageQueue::send() only accepts rvalues; this sends a copy of `value`.
template <class T>
void sendCopy(MessageQueue<T>& queue, T value)
{
    queue.send(std::move(value));
}

// MessageQueue::receive() gives up after ~1 s; this keeps trying until `timeout`.
template <class T>
std::optional<T> receiveWithin(MessageQueue<T>& queue, std::chrono::milliseconds timeout = kAsyncTimeout)
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    do
    {
        auto message = queue.receive();
        if (message.has_value())
        {
            return message;
        }
    } while (std::chrono::steady_clock::now() < deadline);
    return std::nullopt;
}

// Thread-safe observer that remembers every notification it receives.
// Notifications arrive from the Airco worker threads, so all access is locked.
class RecordingObserver : public AircoObserver
{
public:
    void onTargetTemperatureChange(int newTemperature) override
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _targetTemperatures.push_back(newTemperature);
    }
    void onCurrentTemperatureChange(int newTemperature) override
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _currentTemperatures.push_back(newTemperature);
    }
    void onStatusChange(std::string& newStatus) override
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _statuses.push_back(newStatus);
    }

    std::vector<int> targetTemperatures() const
    {
        std::lock_guard<std::mutex> lock(_mutex);
        return _targetTemperatures;
    }
    std::vector<int> currentTemperatures() const
    {
        std::lock_guard<std::mutex> lock(_mutex);
        return _currentTemperatures;
    }
    std::vector<std::string> statuses() const
    {
        std::lock_guard<std::mutex> lock(_mutex);
        return _statuses;
    }
    std::optional<int> lastCurrentTemperature() const
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_currentTemperatures.empty()) return std::nullopt;
        return _currentTemperatures.back();
    }
    std::optional<std::string> lastStatus() const
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_statuses.empty()) return std::nullopt;
        return _statuses.back();
    }
    bool hasSeenCurrentTemperature(int temperature) const
    {
        std::lock_guard<std::mutex> lock(_mutex);
        for (int t : _currentTemperatures)
        {
            if (t == temperature) return true;
        }
        return false;
    }

private:
    mutable std::mutex _mutex;
    std::vector<int> _targetTemperatures;
    std::vector<int> _currentTemperatures;
    std::vector<std::string> _statuses;
};

// gMock version of the observer, for verifying exact interactions.
class MockAircoObserver : public AircoObserver
{
public:
    MOCK_METHOD(void, onTargetTemperatureChange, (int newTemperature), (override));
    MOCK_METHOD(void, onCurrentTemperatureChange, (int newTemperature), (override));
    MOCK_METHOD(void, onStatusChange, (std::string& newStatus), (override));
};

// Removes the consecutive duplicates, e.g. {20, 21, 21, 22} -> {20, 21, 22}.
template <class T>
std::vector<T> withoutRepeats(const std::vector<T>& values)
{
    std::vector<T> result;
    for (const auto& value : values)
    {
        if (result.empty() || result.back() != value)
        {
            result.push_back(value);
        }
    }
    return result;
}

} // namespace airco_test

#endif
