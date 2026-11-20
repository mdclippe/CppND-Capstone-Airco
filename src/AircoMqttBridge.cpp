#include "AircoMqttBridge.h"

#include <cmath>
#include <iostream>
#include <locale>
#include <sstream>
#include <utility>

AircoMqttBridge::AircoMqttBridge(std::shared_ptr<MqttClient> client,
                                 std::shared_ptr<MessageQueue<int>> temperatureQueue,
                                 const std::string& topicPrefix)
    : _client(std::move(client)), _topics(topicsFor(topicPrefix))
{
    // the handler only keeps the queue alive, not the bridge
    _client->subscribe(_topics.sensorTemperature,
        [queue = std::move(temperatureQueue)](const std::string& topic, const std::string& payload) {
            auto temperature = parseTemperature(payload);
            if (temperature.has_value())
            {
                queue->send(std::move(*temperature));
            }
            else
            {
                std::cerr << "MQTT: ignoring invalid temperature '" << payload << "' on " << topic << std::endl;
            }
        });
}

AircoMqttBridge::Topics AircoMqttBridge::topicsFor(const std::string& topicPrefix)
{
    return Topics{
        topicPrefix + "/sensor/temperature",
        topicPrefix + "/status",
        topicPrefix + "/target_temperature",
        topicPrefix + "/current_temperature",
    };
}

std::optional<int> AircoMqttBridge::parseTemperature(const std::string& payload)
{
    const char* whitespace = " \t\r\n";
    const auto first = payload.find_first_not_of(whitespace);
    if (first == std::string::npos)
    {
        return std::nullopt;
    }
    const auto last = payload.find_last_not_of(whitespace);
    const std::string number = payload.substr(first, last - first + 1);

    // only plain decimal numbers : [+-]digits[.digits]
    // (the stream below would also accept e.g. "1e2" or, with MSVC, "0x15")
    std::size_t position = (number[0] == '+' || number[0] == '-') ? 1 : 0;
    bool hasDigits = false;
    bool hasPoint = false;
    for (; position < number.size(); ++position)
    {
        const char c = number[position];
        if (c >= '0' && c <= '9')
        {
            hasDigits = true;
        }
        else if (c == '.' && !hasPoint)
        {
            hasPoint = true;
        }
        else
        {
            return std::nullopt; // e.g. "21abc", "21,5" or "21 22"
        }
    }
    if (!hasDigits)
    {
        return std::nullopt;
    }

    std::istringstream stream(number);
    // always '.' as decimal separator, whatever the locale of the application (e.g. nl_BE)
    stream.imbue(std::locale::classic());
    double value = 0.0;
    if (!(stream >> value) || !std::isfinite(value) || value < kMinTemperature || value > kMaxTemperature)
    {
        return std::nullopt;
    }
    return static_cast<int>(std::lround(value));
}

void AircoMqttBridge::onTargetTemperatureChange(int newTemperature)
{
    _client->publish(_topics.targetTemperature, std::to_string(newTemperature), true);
}

void AircoMqttBridge::onCurrentTemperatureChange(int newTemperature)
{
    _client->publish(_topics.currentTemperature, std::to_string(newTemperature), true);
}

void AircoMqttBridge::onStatusChange(std::string& newStatus)
{
    _client->publish(_topics.status, newStatus, true);
}
