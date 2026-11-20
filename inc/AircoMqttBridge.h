#ifndef AIRCO_MQTT_BRIDGE_H
#define AIRCO_MQTT_BRIDGE_H

#include <memory>
#include <optional>
#include <string>

#include "AircoObserver.h"
#include "MessageQueue.h"
#include "MqttClient.h"

// Connects the airco to MQTT, like Monitor connects it to Qt :
//  - temperature readings received on <prefix>/sensor/temperature go to the
//    airco's temperature queue (so a sensor anywhere on the network can feed it)
//  - as an AircoObserver it publishes the status, target and current temperature
//    (retained, so a new dashboard immediately gets the last known values)
//
// Topics (prefix "airco" by default):
//   <prefix>/sensor/temperature    in  : "21" or "21.4" (rounded), other payloads are ignored
//   <prefix>/status                out : OFF / STANDBY / HEATING / COOLING
//   <prefix>/target_temperature    out : e.g. "22"
//   <prefix>/current_temperature   out : last reading handled by the airco, e.g. "21"
class AircoMqttBridge : public AircoObserver {
public:
    struct Topics {
        std::string sensorTemperature;
        std::string status;
        std::string targetTemperature;
        std::string currentTemperature;
    };

    // readings outside this range are considered sensor errors and ignored
    static constexpr int kMinTemperature = -100;
    static constexpr int kMaxTemperature = 100;

    AircoMqttBridge(std::shared_ptr<MqttClient> client,
                    std::shared_ptr<MessageQueue<int>> temperatureQueue,
                    const std::string& topicPrefix = "airco");

    static Topics topicsFor(const std::string& topicPrefix);
    const Topics& topics() const { return _topics; }

    // "21" -> 21, " 21.6 " -> 22, anything that is not a plausible temperature -> nullopt
    static std::optional<int> parseTemperature(const std::string& payload);

    void onTargetTemperatureChange(int newTemperature) override;
    void onCurrentTemperatureChange(int newTemperature) override;
    void onStatusChange(std::string& newStatus) override;

private:
    std::shared_ptr<MqttClient> _client;
    Topics _topics;
};

#endif
