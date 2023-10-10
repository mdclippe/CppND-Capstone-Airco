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
    std::shared_ptr<TemperatureNode> node1 = std::make_shared<TemperatureNode>(msgQueue,1);
    
    std::shared_ptr<Airco> airco = std::make_shared<Airco>(msgQueue);
    std::shared_ptr<Monitor> monitor = std::make_shared<Monitor>();
    airco->addObserver(monitor);
    airco->addObserver(node1);

    QApplication app(argc, argv);

    MainWindow w(monitor, airco, node1);
    w.show();
    return app.exec();
}
