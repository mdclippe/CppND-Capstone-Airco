# ---------------------------------------------------------------------------
# GoogleTest (fetched at configure time, pinned version)
# ---------------------------------------------------------------------------
include(FetchContent)
include(GoogleTest)

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
)
# MSVC: use the same (dynamic) C runtime as the rest of the project
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# The tests do not declare any Q_OBJECT classes themselves
set(CMAKE_AUTOMOC OFF)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTORCC OFF)

# ---------------------------------------------------------------------------
# airco_add_test_suite(<target>
#     LABEL <unit|integration|system>
#     SOURCES <files...>                 tests that only need airco_core
#     [QT_SOURCES <files...>]            tests that need Qt (only built with AIRCO_BUILD_GUI)
#     [MQTT_SOURCES <files...>]          tests that need Paho MQTT (only built with AIRCO_WITH_MQTT)
#     [NEEDS_QAPPLICATION])              create a QApplication in main()
#
# Every GoogleTest test case becomes a separate CTest test carrying LABEL,
# so a category can be run with `ctest -L <label>`.
# ---------------------------------------------------------------------------
function(airco_add_test_suite target)
    cmake_parse_arguments(ARG "NEEDS_QAPPLICATION" "LABEL" "SOURCES;QT_SOURCES;MQTT_SOURCES" ${ARGN})

    add_executable(${target}
        ${ARG_SOURCES}
        support/test_main.cpp
        support/TestHelpers.h
        support/FakeMqtt.h
    )
    target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/support)
    target_link_libraries(${target} PRIVATE airco_core GTest::gmock)
    if(NOT MSVC)
        target_compile_options(${target} PRIVATE -Wall)
    endif()

    if(AIRCO_BUILD_GUI AND ARG_QT_SOURCES)
        target_sources(${target} PRIVATE ${ARG_QT_SOURCES})
        target_link_libraries(${target} PRIVATE airco_gui)
        if(ARG_NEEDS_QAPPLICATION)
            target_compile_definitions(${target} PRIVATE AIRCO_TESTS_NEED_QAPPLICATION)
        endif()
        # GUI tests run headless where the offscreen platform plugin is available
        # (always on Linux/macOS, deployed by windeployqt on Windows with Qt >= 6.5)
        if(NOT WIN32 OR QT_VERSION VERSION_GREATER_EQUAL 6.5)
            target_compile_definitions(${target} PRIVATE AIRCO_TESTS_QT_OFFSCREEN)
        endif()
        # Windows: the test executable runs at build time for the test discovery,
        # so it needs the Qt DLLs next to it (must come before gtest_discover_tests)
        airco_deploy_qt(${target} OFFSCREEN)
    endif()

    if(AIRCO_WITH_MQTT AND ARG_MQTT_SOURCES)
        target_sources(${target} PRIVATE ${ARG_MQTT_SOURCES} support/MqttBrokerTestSupport.h)
        target_link_libraries(${target} PRIVATE airco_mqtt)
    endif()

    gtest_discover_tests(${target}
        PROPERTIES LABELS "${ARG_LABEL}" TIMEOUT 120
        DISCOVERY_TIMEOUT 60
    )
endfunction()

# ---------------------------------------------------------------------------
# Unit tests : one class at a time, collaborators replaced by mocks / fakes
# ---------------------------------------------------------------------------
airco_add_test_suite(airco_unit_tests
    LABEL unit
    SOURCES
        unit/MessageQueueTest.cpp
        unit/NodeTest.cpp
        unit/TemperatureNodeTest.cpp
        unit/AircoTest.cpp
        unit/AircoMqttBridgeTest.cpp
    QT_SOURCES
        unit/MonitorTest.cpp
)

# ---------------------------------------------------------------------------
# Integration tests : real components working together
# ---------------------------------------------------------------------------
airco_add_test_suite(airco_integration_tests
    LABEL integration
    SOURCES
        integration/AircoTemperatureNodeIntegrationTest.cpp
        integration/AircoMqttIntegrationTest.cpp
    QT_SOURCES
        integration/AircoMonitorIntegrationTest.cpp
    MQTT_SOURCES
        integration/PahoMqttClientIntegrationTest.cpp
)

# ---------------------------------------------------------------------------
# System tests : the complete application, wired like main.cpp,
# driven through user scenarios (and through the GUI when Qt is available)
# ---------------------------------------------------------------------------
airco_add_test_suite(airco_system_tests
    LABEL system
    SOURCES
        system/AircoSystemTest.cpp
    QT_SOURCES
        system/MainWindowSystemTest.cpp
    MQTT_SOURCES
        system/AircoMqttSystemTest.cpp
    NEEDS_QAPPLICATION
)
