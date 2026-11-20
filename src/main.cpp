#include <iostream>
#include <QApplication>

#include "MessageQueue.h"
#include "TemperatureNode.h"
#include "Monitor.h"
#include "Airco.h"
#include "MainWindow.h"

#ifdef AIRCO_WITH_MQTT
#include <random>
#include "AircoMqttBridge.h"
#include "PahoMqttClient.h"
#endif


int main(int argc, char** argv)
{
    std::shared_ptr<MessageQueue<int>> msgQueue = std::make_shared<MessageQueue<int>>(kTemperatureQueueCapacity);
    std::shared_ptr<TemperatureNode> node1 = std::make_shared<TemperatureNode>(msgQueue,1);

    std::shared_ptr<Airco> airco = std::make_shared<Airco>(msgQueue);
    std::shared_ptr<Monitor> monitor = std::make_shared<Monitor>();
    airco->addObserver(monitor);
    airco->addObserver(node1);

#ifdef AIRCO_WITH_MQTT
    // MQTT : temperature readings in, airco status out (see README).
    // Without a reachable broker the airco simply runs without MQTT.
    const std::string broker = qEnvironmentVariable("AIRCO_MQTT_BROKER", "tcp://localhost:1883").toStdString();
    const std::string topicPrefix = qEnvironmentVariable("AIRCO_MQTT_TOPIC_PREFIX", "airco").toStdString();
    const std::string clientId = "airco-" + std::to_string(std::random_device{}());

    auto mqttClient = std::make_shared<PahoMqttClient>(broker, clientId);
    if (mqttClient->connect())
    {
        auto mqttBridge = std::make_shared<AircoMqttBridge>(mqttClient, msgQueue, topicPrefix);
        airco->addObserver(mqttBridge);
        std::cout << "MQTT: connected to " << broker << ", readings on "
                  << mqttBridge->topics().sensorTemperature << std::endl;
    }
    else
    {
        std::cout << "MQTT: no broker at " << broker << ", continuing without MQTT" << std::endl;
    }
#endif

    QApplication app(argc, argv);

    MainWindow w(monitor, airco, node1);
    w.show();
    return app.exec();
}
