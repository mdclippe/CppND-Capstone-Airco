#include "PahoMqttClient.h"

#include <iostream>
#include <utility>
#include <vector>

PahoMqttClient::PahoMqttClient(const std::string& serverUri, const std::string& clientId)
    : _client(serverUri, clientId)
{
    // called after the first connect and after every automatic reconnect
    _client.set_connected_handler([this](const std::string&) { subscribeAll(); });
    _client.set_message_callback([this](mqtt::const_message_ptr message) { onMessage(message); });
}

PahoMqttClient::~PahoMqttClient()
{
    try
    {
        if (_client.is_connected())
        {
            _client.disconnect()->wait_for(std::chrono::seconds(2));
        }
    }
    catch (const mqtt::exception& e)
    {
        std::cerr << "MQTT: disconnect failed: " << e.what() << std::endl;
    }
}

bool PahoMqttClient::connect(std::chrono::seconds timeout)
{
    auto options = mqtt::connect_options_builder()
                       .clean_session(true)
                       .connect_timeout(timeout)
                       .automatic_reconnect(std::chrono::seconds(1), std::chrono::seconds(30))
                       .finalize();
    try
    {
        // wait a little longer than the connect timeout, Paho reports the failure itself
        return _client.connect(options)->wait_for(timeout + std::chrono::seconds(1));
    }
    catch (const mqtt::exception& e)
    {
        std::cerr << "MQTT: cannot connect to " << _client.get_server_uri() << ": " << e.what() << std::endl;
        return false;
    }
}

bool PahoMqttClient::isConnected() const
{
    return _client.is_connected();
}

void PahoMqttClient::publish(const std::string& topic, const std::string& payload, bool retained)
{
    if (!_client.is_connected())
    {
        return;
    }
    try
    {
        // asynchronous : don't wait for the broker, this is called from the airco threads
        _client.publish(topic, payload.data(), payload.size(), kQos, retained);
    }
    catch (const mqtt::exception& e)
    {
        std::cerr << "MQTT: publish on " << topic << " failed: " << e.what() << std::endl;
    }
}

void PahoMqttClient::subscribe(const std::string& topic, MessageHandler handler)
{
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _handlers[topic] = std::move(handler);
    }
    if (_client.is_connected())
    {
        try
        {
            _client.subscribe(topic, kQos);
        }
        catch (const mqtt::exception& e)
        {
            std::cerr << "MQTT: subscribe to " << topic << " failed: " << e.what() << std::endl;
        }
    }
}

void PahoMqttClient::subscribeAll()
{
    std::vector<std::string> topics;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        for (const auto& entry : _handlers)
        {
            topics.push_back(entry.first);
        }
    }
    for (const auto& topic : topics)
    {
        try
        {
            _client.subscribe(topic, kQos);
        }
        catch (const mqtt::exception& e)
        {
            std::cerr << "MQTT: subscribe to " << topic << " failed: " << e.what() << std::endl;
        }
    }
}

void PahoMqttClient::onMessage(mqtt::const_message_ptr message)
{
    MessageHandler handler;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _handlers.find(message->get_topic());
        if (it == _handlers.end())
        {
            return;
        }
        handler = it->second;
    }
    // called without holding the lock, the handler may take its time
    handler(message->get_topic(), message->to_string());
}
