// System tests through the real Qt user interface (offscreen by default on Linux/macOS).
// Only built when the Qt GUI is enabled; test_main.cpp creates the QApplication.
#include <gmock/gmock.h>

#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <QApplication>
#include <QLCDNumber>
#include <QPushButton>
#include <QSlider>
#include <QTextEdit>

#include "MainWindow.h"
#include "TestHelpers.h"

using namespace std::chrono_literals;
using ::testing::ElementsAre;

namespace {

class MainWindowSystemTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_NE(nullptr, QApplication::instance()) << "test_main.cpp should create a QApplication";

        // same wiring as main.cpp
        airco->addObserver(monitor);
        airco->addObserver(node1);
        window = std::make_unique<MainWindow>(monitor, airco, node1);
        window->show();

        powerButton    = findWidget<QPushButton>("btnON");
        autoButton     = findWidget<QPushButton>("btnAUTO");
        coolButton     = findWidget<QPushButton>("btnCOOL");
        heatButton     = findWidget<QPushButton>("btnHEAT");
        simulateButton = findWidget<QPushButton>("btnSimulateTemp");
        targetSlider   = findWidget<QSlider>("sldTargetTemp");
        targetDisplay  = findWidget<QLCDNumber>("lcdTargetTemp");
        currentDisplay = findWidget<QLCDNumber>("lcdCurrentTemp");
        statusDisplay  = findWidget<QTextEdit>("txtAircoStatus");
        ASSERT_FALSE(HasFailure()) << "the window is missing widgets";

        // remember every status the user got to see
        QObject::connect(statusDisplay, &QTextEdit::textChanged, statusDisplay, [this] {
            const auto text = statusDisplay->toPlainText();
            if (!text.isEmpty())
            {
                shownStatuses.push_back(text.toStdString());
            }
        });
    }

    void TearDown() override
    {
        node1->stopSimulate();
        window.reset();
    }

    template <class Widget>
    Widget* findWidget(const char* name)
    {
        auto* widget = window->findChild<Widget*>(name);
        if (widget == nullptr)
        {
            ADD_FAILURE() << "widget not found: " << name;
        }
        return widget;
    }

    // Lets Qt deliver the (queued) signals from the airco threads until `condition` holds
    template <class Condition>
    bool processEventsUntil(Condition condition, std::chrono::milliseconds timeout = 5s)
    {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline)
        {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
            if (condition())
            {
                return true;
            }
            std::this_thread::sleep_for(2ms);
        }
        return condition();
    }

    std::string shownStatus() const { return statusDisplay->toPlainText().toStdString(); }

    // MainWindow keeps references to these, so they must outlive the window
    std::shared_ptr<MessageQueue<int>> msgQueue = std::make_shared<MessageQueue<int>>();
    std::shared_ptr<TemperatureNode> node1 = std::make_shared<TemperatureNode>(msgQueue, 1, 50ms);
    std::shared_ptr<Airco> airco = std::make_shared<Airco>(msgQueue);
    std::shared_ptr<Monitor> monitor = std::make_shared<Monitor>();
    std::unique_ptr<MainWindow> window;

    QPushButton* powerButton = nullptr;
    QPushButton* autoButton = nullptr;
    QPushButton* coolButton = nullptr;
    QPushButton* heatButton = nullptr;
    QPushButton* simulateButton = nullptr;
    QSlider* targetSlider = nullptr;
    QLCDNumber* targetDisplay = nullptr;
    QLCDNumber* currentDisplay = nullptr;
    QTextEdit* statusDisplay = nullptr;
    std::vector<std::string> shownStatuses;
};

} // namespace

TEST_F(MainWindowSystemTest, StartsWithTheSliderTargetAndRoomTemperatureOf20)
{
    EXPECT_EQ(20, targetSlider->value());
    EXPECT_EQ(20, airco->getTargetTemperature());
    EXPECT_EQ(20, targetDisplay->intValue());
    EXPECT_EQ(20, currentDisplay->intValue());
    EXPECT_EQ(AircoStates::OFF, airco->getCurrentState());
    EXPECT_EQ("ON", powerButton->text().toStdString());
    EXPECT_TRUE(autoButton->isChecked());
}

TEST_F(MainWindowSystemTest, PowerButtonSwitchesTheAircoOnAndOff)
{
    powerButton->click();

    EXPECT_EQ(AircoStates::STANDBY, airco->getCurrentState());
    EXPECT_EQ("OFF", powerButton->text().toStdString());
    EXPECT_TRUE(processEventsUntil([&] { return shownStatus() == "STANDBY"; }));

    powerButton->click();

    EXPECT_EQ(AircoStates::OFF, airco->getCurrentState());
    EXPECT_EQ("ON", powerButton->text().toStdString());
    EXPECT_TRUE(processEventsUntil([&] { return shownStatus() == "OFF"; }));
}

TEST_F(MainWindowSystemTest, SliderSetsTheTargetTemperature)
{
    targetSlider->setValue(25);

    EXPECT_EQ(25, airco->getTargetTemperature());
    EXPECT_TRUE(processEventsUntil([&] { return targetDisplay->intValue() == 25; }));
}

TEST_F(MainWindowSystemTest, ModeButtonsSwitchTheAircoMode)
{
    coolButton->click();
    EXPECT_TRUE(processEventsUntil([&] { return airco->getMode() == AircoModes::MANUAL_COOL; }));
    EXPECT_TRUE(coolButton->isChecked());
    EXPECT_FALSE(autoButton->isChecked());

    heatButton->click();
    EXPECT_TRUE(processEventsUntil([&] { return airco->getMode() == AircoModes::MANUAL_HEAT; }));
    EXPECT_TRUE(heatButton->isChecked());
    EXPECT_FALSE(coolButton->isChecked());

    autoButton->click();
    EXPECT_TRUE(processEventsUntil([&] { return airco->getMode() == AircoModes::AUTO; }));
    EXPECT_TRUE(autoButton->isChecked());
}

TEST_F(MainWindowSystemTest, UserWarmsUpTheRoomThroughTheUserInterface)
{
    powerButton->click();
    targetSlider->setValue(22);

    simulateButton->click();
    EXPECT_EQ("Simulate OFF", simulateButton->text().toStdString());

    ASSERT_TRUE(processEventsUntil([&] {
        return currentDisplay->intValue() == 22 && shownStatus() == "STANDBY";
    }));
    EXPECT_THAT(shownStatuses, ElementsAre("STANDBY", "HEATING", "STANDBY"));

    simulateButton->click();
    EXPECT_EQ("Simulate ON", simulateButton->text().toStdString());
}
