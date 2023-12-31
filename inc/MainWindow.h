#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "Monitor.h"
#include "TemperatureNode.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(std::shared_ptr<Monitor>& moni,std::shared_ptr<Airco>& airco,std::shared_ptr<TemperatureNode>& node, QWidget *parent = nullptr);
    ~MainWindow();
private slots:
    void on_sldTargetTemp_valueChanged(int value);
    void updateStatus(const QString& newStatus);
    void on_btnAUTO_toggled(bool checked);  
    void on_btnON_toggled(bool checked);
    void on_btnCOOL_toggled(bool checked);

    void on_btnHEAT_toggled(bool checked);
    void on_btnSimulateTemp_toggled(bool checked);
private:
    Ui::MainWindow *ui;
    std::shared_ptr<Monitor>& _monitor;
    std::shared_ptr<Airco>& _airco;
    std::shared_ptr<TemperatureNode>& _temperatureNode;
};
#endif // MAINWINDOW_H
