#ifndef PAHO_MQTT_CLIENT_H
#define PAHO_MQTT_CLIENT_H

#include <chrono>
#include <map>
#include <mutex>
#include <string>

#include <mqtt/async_client.h>

#include "MqttClient.h"

// MqttClient implemented with the Eclipse Paho MQTT C++ library.
// After a successful connect() the client reconnects automatically when the
// connection is lost and subscribes to its topics again.
class PahoMqttClient : public MqttClient {
public:
    // serverUri e.g. "tcp://localhost:1883", clientId must be unique on the broker
    PahoMqttClient(const std::string& serverUri, const std::string& clientId);
    ~PahoMqttClient() override;

    PahoMqttClient(const PahoMqttClient&) = delete;
    PahoMqttClient& operator=(const PahoMqttClient&) = delete;

    // Returns false when the broker cannot be reached within `timeout`.
    bool connect(std::chrono::seconds timeout = std::chrono::seconds(5));
    bool isConnected() const;

    // Messages published while not connected are dropped.
    void publish(const std::string& topic, const std::string& payload, bool retained) override;
    // Subscriptions are remembered and renewed after every (re)connect.
    void subscribe(const std::string& topic, MessageHandler handler) override;

private:
    void subscribeAll();
    void onMessage(mqtt::const_message_ptr message);

    static constexpr int kQos = 1;

    // declared before _client : the client (and its callback threads) must go first
    std::mutex _mutex;
    std::map<std::string, MessageHandler> _handlers;
    mqtt::async_client _client;
};

#endif
