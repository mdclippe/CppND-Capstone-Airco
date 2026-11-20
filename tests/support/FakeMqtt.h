#ifndef AIRCO_FAKE_MQTT_H
#define AIRCO_FAKE_MQTT_H

#include <gmock/gmock.h>

#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "MqttClient.h"

namespace airco_test {

// gMock version of the MQTT client, for verifying exact publishes / subscribes.
class MockMqttClient : public MqttClient
{
public:
    MOCK_METHOD(void, publish, (const std::string& topic, const std::string& payload, bool retained), (override));
    MOCK_METHOD(void, subscribe, (const std::string& topic, MessageHandler handler), (override));
};

// In-memory stand-in for an MQTT broker. Its clients deliver every publish
// synchronously to the subscribers of that exact topic, and retained messages
// are handed to subscribers that come later, like a real broker does.
// The broker must outlive the clients it created.
class FakeMqttBroker
{
public:
    std::shared_ptr<MqttClient> createClient() { return std::make_shared<Client>(*this); }

    // publish as an external device would (a sensor, mosquitto_pub, ...)
    void publish(const std::string& topic, const std::string& payload, bool retained = false)
    {
        std::vector<MqttClient::MessageHandler> handlers;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _published[topic].push_back(payload);
            if (retained)
            {
                _retained[topic] = payload;
            }
            auto range = _subscribers.equal_range(topic);
            for (auto it = range.first; it != range.second; ++it)
            {
                handlers.push_back(it->second);
            }
        }
        for (auto& handler : handlers)
        {
            handler(topic, payload);
        }
    }

    void subscribe(const std::string& topic, MqttClient::MessageHandler handler)
    {
        std::optional<std::string> retained;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _subscribers.emplace(topic, handler);
            auto it = _retained.find(topic);
            if (it != _retained.end())
            {
                retained = it->second;
            }
        }
        if (retained.has_value())
        {
            handler(topic, *retained);
        }
    }

    std::optional<std::string> retainedMessage(const std::string& topic) const
    {
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _retained.find(topic);
        if (it == _retained.end()) return std::nullopt;
        return it->second;
    }

    // every payload published on `topic`, in order
    std::vector<std::string> payloadsOn(const std::string& topic) const
    {
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _published.find(topic);
        if (it == _published.end()) return {};
        return it->second;
    }

    bool hasSubscriber(const std::string& topic) const
    {
        std::lock_guard<std::mutex> lock(_mutex);
        return _subscribers.count(topic) > 0;
    }

private:
    class Client : public MqttClient
    {
    public:
        explicit Client(FakeMqttBroker& broker) : _broker(broker) {}
        void publish(const std::string& topic, const std::string& payload, bool retained) override
        {
            _broker.publish(topic, payload, retained);
        }
        void subscribe(const std::string& topic, MessageHandler handler) override
        {
            _broker.subscribe(topic, std::move(handler));
        }

    private:
        FakeMqttBroker& _broker;
    };

    mutable std::mutex _mutex;
    std::multimap<std::string, MqttClient::MessageHandler> _subscribers;
    std::map<std::string, std::string> _retained;
    std::map<std::string, std::vector<std::string>> _published;
};

} // namespace airco_test

#endif
