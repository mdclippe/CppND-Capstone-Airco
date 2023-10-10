#include "MainWindow.h"
#include "./ui_mainwindow.h"
#include "Monitor.h"

MainWindow::MainWindow( std::shared_ptr<Monitor>& moni, std::shared_ptr<Airco>& airco, std::shared_ptr<TemperatureNode>& node, QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , _monitor(moni), _airco(airco), _temperatureNode(node)
{
    ui->setupUi(this);
    QObject::connect(_monitor.get(), &Monitor::targetTemperatureChanged, ui->lcdTargetTemp,QOverload<double>::of(&QLCDNumber::display));
    QObject::connect(_monitor.get(), &Monitor::currentTemperatureChanged, ui->lcdCurrentTemp,QOverload<double>::of(&QLCDNumber::display));
    QObject::connect(_monitor.get(), &Monitor::aircoStatusChanged, this, &MainWindow::updateStatus);
    _airco->setTargetTemperature(ui->sldTargetTemp->value());
    _temperatureNode->setValue(20);
    _monitor->onCurrentTemperatureChange(20);
}


void MainWindow::on_sldTargetTemp_valueChanged(int value)
{
    _airco->setTargetTemperature(value);

}
void MainWindow::on_btnON_toggled(bool checked)
{
    if (checked)
    {
    ui->btnON->setText("OFF");
    }
    else 
    {
        ui->btnON->setText("ON");
    }
    _airco->togglePower();
}

void MainWindow::on_btnSimulateTemp_toggled(bool checked)
{
    if (checked)
    {
        ui->btnSimulateTemp->setText("Simulate OFF");
        _temperatureNode->startSimulate();
    }   
    else
    {
        ui->btnSimulateTemp->setText("Simulate ON");
        _temperatureNode->stopSimulate();
    }
}

void MainWindow::on_btnAUTO_toggled(bool checked)
{
    _airco->handleEvent("auto_requested");
}

void MainWindow::on_btnCOOL_toggled(bool checked)
{
    _airco->handleEvent("cooling_requested");
}


void MainWindow::on_btnHEAT_toggled(bool checked)
{
    _airco->handleEvent("heating_requested");
}

void MainWindow::updateStatus(const QString& newStatus){
    ui->txtAircoStatus->clear();
    ui->txtAircoStatus->setText(newStatus);
}

MainWindow::~MainWindow()
{
    delete ui;
}

