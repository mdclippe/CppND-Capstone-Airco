// Shared main() for all test executables.
#include <gmock/gmock.h>

#ifdef AIRCO_TESTS_NEED_QAPPLICATION
#include <QApplication>
#include <memory>
#endif

int main(int argc, char** argv)
{
    ::testing::InitGoogleMock(&argc, argv);

#ifdef AIRCO_TESTS_NEED_QAPPLICATION
#ifdef AIRCO_TESTS_QT_OFFSCREEN
    // GUI tests run headless unless a platform is chosen explicitly
    if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM"))
    {
        qputenv("QT_QPA_PLATFORM", "offscreen");
    }
#endif
    // Listing the tests (CTest discovery at build time) must not need a GUI
    std::unique_ptr<QApplication> app;
    if (!GTEST_FLAG_GET(list_tests))
    {
        app = std::make_unique<QApplication>(argc, argv);
    }
#endif

    return RUN_ALL_TESTS();
}
