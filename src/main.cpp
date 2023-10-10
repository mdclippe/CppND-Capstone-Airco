#include <iostream>
#include <QApplication>

#include "MessageQueue.h"
#include "TemperatureNode.h"
#include "Monitor.h"
#include "Airco.h"
#include "MainWindow.h"


int main(int argc, char** argv)
{
    std::shared_ptr<MessageQueue<int>> msgQueue = std::make_shared<MessageQueue<int>>();    
    TemperatureNode node1(msgQueue,1);
    
    
    Airco airco1(msgQueue);
    Monitor monitor;
    airco1.addObserver(&monitor);
    airco1.addObserver(&node1);

    QApplication app(argc, argv);

    MainWindow w(std::move(monitor), airco1, node1);
    w.show();
    return app.exec();
}
