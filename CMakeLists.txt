cmake_minimum_required(VERSION 3.16)

project(Airco VERSION 0.1
                  DESCRIPTION "Airco simulation"
                  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(AIRCO_BUILD_GUI   "Build the Qt GUI application (requires Qt)" ON)
option(AIRCO_BUILD_TESTS "Build the GoogleTest unit, integration and system tests" ON)
option(AIRCO_WITH_MQTT   "Connect the airco to an MQTT broker (Eclipse Paho MQTT C++)" ON)

find_package(Threads REQUIRED)

# ---------------------------------------------------------------------------
# airco_core : the airco logic without any Qt dependency.
# Used by the GUI application and by the tests.
# ---------------------------------------------------------------------------
add_library(airco_core STATIC
    src/Airco.cpp
    src/AircoMqttBridge.cpp
    src/MessageQueue.cpp
    src/Node.cpp
    src/TemperatureNode.cpp
    inc/Airco.h
    inc/AircoMqttBridge.h
    inc/AircoObserver.h
    inc/MessageQueue.h
    inc/MqttClient.h
    inc/Node.h
    inc/TemperatureNode.h
)
target_include_directories(airco_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/inc)
target_link_libraries(airco_core PUBLIC Threads::Threads)
if(NOT MSVC)
    target_compile_options(airco_core PRIVATE -Wall)
endif()

# ---------------------------------------------------------------------------
# MQTT : airco_mqtt = the MqttClient interface implemented with Eclipse Paho.
# Uses an installed Paho (e.g. apt install libpaho-mqttpp-dev), otherwise it
# is downloaded and built together with the bundled Paho C library.
# ---------------------------------------------------------------------------
if(AIRCO_WITH_MQTT)
    find_package(PahoMqttCpp CONFIG QUIET)
    if(PahoMqttCpp_FOUND)
        message(STATUS "Paho MQTT C++ ${PahoMqttCpp_VERSION}: installed version")
    else()
        message(STATUS "Paho MQTT C++: not installed, building v1.6.0 from source")
        include(FetchContent)
        FetchContent_Declare(paho_mqtt_cpp
            GIT_REPOSITORY https://github.com/eclipse-paho/paho.mqtt.cpp.git
            GIT_TAG        v1.6.0
            GIT_SHALLOW    TRUE
            GIT_SUBMODULES externals/paho.mqtt.c
        )
        set(PAHO_WITH_MQTT_C          ON  CACHE BOOL "" FORCE)
        set(PAHO_BUILD_STATIC         ON  CACHE BOOL "" FORCE)
        set(PAHO_BUILD_SHARED         OFF CACHE BOOL "" FORCE)
        set(PAHO_WITH_SSL             OFF CACHE BOOL "" FORCE)
        set(PAHO_BUILD_EXAMPLES       OFF CACHE BOOL "" FORCE)
        set(PAHO_BUILD_SAMPLES        OFF CACHE BOOL "" FORCE)
        set(PAHO_BUILD_TESTS          OFF CACHE BOOL "" FORCE)
        set(PAHO_BUILD_DOCUMENTATION  OFF CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(paho_mqtt_cpp)
    endif()

    add_library(airco_mqtt STATIC
        src/PahoMqttClient.cpp
        inc/PahoMqttClient.h
    )
    target_link_libraries(airco_mqtt PUBLIC airco_core PahoMqttCpp::paho-mqttpp3)
    target_compile_definitions(airco_mqtt PUBLIC AIRCO_WITH_MQTT)
    if(NOT MSVC)
        target_compile_options(airco_mqtt PRIVATE -Wall)
    endif()
endif()

# ---------------------------------------------------------------------------
# Qt GUI : airco_gui library (Monitor + MainWindow) and the Airco executable
# ---------------------------------------------------------------------------
if(AIRCO_BUILD_GUI)
    # set(CMAKE_PREFIX_PATH "/home/mdclippe/Qt/6.5.2/gcc_64")
    set(CMAKE_AUTOUIC ON)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)

    find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

    # airco_deploy_qt(<target> [OFFSCREEN])
    # Windows: copy the Qt DLLs, plugins and (MinGW) compiler runtime next to the
    # executable, so it also runs outside Qt Creator (VS Code, CTest, Explorer).
    # OFFSCREEN also deploys the platform plugin for headless GUI tests.
    function(airco_deploy_qt target)
        cmake_parse_arguments(ARG "OFFSCREEN" "" "" ${ARGN})
        if(NOT WIN32 OR NOT TARGET Qt${QT_VERSION_MAJOR}::windeployqt)
            return()
        endif()
        set(options --no-translations --verbose 0)
        if(MINGW)
            list(APPEND options --compiler-runtime)
        else()
            list(APPEND options --no-compiler-runtime)
        endif()
        if(ARG_OFFSCREEN AND QT_VERSION VERSION_GREATER_EQUAL 6.5)
            list(APPEND options --include-plugins qoffscreen)
        endif()
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND Qt${QT_VERSION_MAJOR}::windeployqt ${options} $<TARGET_FILE:${target}>
            VERBATIM
        )
    endfunction()

    add_library(airco_gui STATIC
        src/Monitor.cpp
        src/MainWindow.cpp
        src/mainwindow.ui
        inc/Monitor.h
        inc/MainWindow.h
    )
    target_link_libraries(airco_gui PUBLIC airco_core Qt${QT_VERSION_MAJOR}::Widgets)

    if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
        qt_add_executable(Airco
            MANUAL_FINALIZATION
            src/main.cpp
        )
    # Define target properties for Android with Qt 6 as:
    #    set_property(TARGET Airco APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
    #                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
    # For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
    else()
        if(ANDROID)
            add_library(Airco SHARED
                src/main.cpp
            )
    # Define properties for Android with Qt 5 after find_package() calls as:
    #    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
        else()
            add_executable(Airco
                src/main.cpp
            )
        endif()
    endif()

    target_link_libraries(Airco PRIVATE airco_gui)
    if(AIRCO_WITH_MQTT)
        target_link_libraries(Airco PRIVATE airco_mqtt)
    endif()
    airco_deploy_qt(Airco)

    # Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
    # If you are developing for iOS or macOS you should consider setting an
    # explicit, fixed bundle identifier manually though.
    if(${QT_VERSION} VERSION_LESS 6.1.0)
      set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.remotecontrol)
    endif()
    set_target_properties(Airco PROPERTIES
        ${BUNDLE_ID_OPTION}
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
        MACOSX_BUNDLE TRUE
        WIN32_EXECUTABLE TRUE
    )

    include(GNUInstallDirs)
    install(TARGETS Airco
        BUNDLE DESTINATION .
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )

    if(QT_VERSION_MAJOR EQUAL 6)
        qt_finalize_executable(Airco)
    endif()
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(AIRCO_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
