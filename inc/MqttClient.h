#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include <functional>
#include <string>

// Minimal MQTT client interface used by the airco code.
// PahoMqttClient implements it with Eclipse Paho, the tests use fakes and mocks,
// so the airco logic does not depend on a broker or on the MQTT library.
class MqttClient {
public:
    using MessageHandler = std::function<void(const std::string& topic, const std::string& payload)>;

    virtual ~MqttClient() = default;

    // Publishes `payload` on `topic`. A retained message is kept by the broker
    // and delivered to everyone who subscribes later.
    virtual void publish(const std::string& topic, const std::string& payload, bool retained) = 0;

    // Calls `handler` for every message received on `topic`.
    // The handler runs on a thread of the MQTT client.
    virtual void subscribe(const std::string& topic, MessageHandler handler) = 0;
};

#endif
