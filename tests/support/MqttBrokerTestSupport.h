#ifndef AIRCO_MQTT_BROKER_TEST_SUPPORT_H
#define AIRCO_MQTT_BROKER_TEST_SUPPORT_H

// Support for the tests that need a real MQTT broker (e.g. Mosquitto).
// The broker is taken from AIRCO_TEST_MQTT_BROKER (default tcp://localhost:1883);
// without a reachable broker these tests are skipped.

#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <random>
#include <string>

#include "PahoMqttClient.h"

namespace airco_test {

inline std::string environmentOr(const char* name, const std::string& fallback)
{
#ifdef _MSC_VER
    // MSVC deprecates std::getenv
    char* value = nullptr;
    size_t length = 0;
    std::string result = fallback;
    if (_dupenv_s(&value, &length, name) == 0 && value != nullptr)
    {
        if (*value != '\0') result = value;
        std::free(value);
    }
    return result;
#else
    const char* value = std::getenv(name);
    return (value != nullptr && *value != '\0') ? std::string(value) : fallback;
#endif
}

inline std::string testBrokerUri()
{
    return environmentOr("AIRCO_TEST_MQTT_BROKER", "tcp://localhost:1883");
}

// client ids and topic prefixes must not collide between test runs
inline std::string uniqueName(const std::string& base)
{
    static std::mt19937 generator{std::random_device{}()};
    return base + "-" + std::to_string(generator());
}

inline bool testBrokerAvailable()
{
    static const bool available = [] {
        PahoMqttClient probe(testBrokerUri(), uniqueName("airco-probe"));
        return probe.connect(std::chrono::seconds(2));
    }();
    return available;
}

} // namespace airco_test

#define SKIP_WITHOUT_MQTT_BROKER()                                                       \
    if (!::airco_test::testBrokerAvailable())                                            \
    GTEST_SKIP() << "no MQTT broker at " << ::airco_test::testBrokerUri()                \
                 << " (start one, e.g. mosquitto, or set AIRCO_TEST_MQTT_BROKER)"

#endif
