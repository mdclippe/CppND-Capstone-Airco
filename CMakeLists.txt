cmake_minimum_required(VERSION 3.16)

project(Airco VERSION 0.1
                  DESCRIPTION "Airco simulation"
                  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(AIRCO_BUILD_GUI   "Build the Qt GUI application (requires Qt)" ON)
option(AIRCO_BUILD_TESTS "Build the GoogleTest unit, integration and system tests" ON)

find_package(Threads REQUIRED)

# ---------------------------------------------------------------------------
# airco_core : the airco logic without any Qt dependency.
# Used by the GUI application and by the tests.
# ---------------------------------------------------------------------------
add_library(airco_core STATIC
    src/Airco.cpp
    src/MessageQueue.cpp
    src/Node.cpp
    src/TemperatureNode.cpp
    inc/Airco.h
    inc/AircoObserver.h
    inc/MessageQueue.h
    inc/Node.h
    inc/TemperatureNode.h
)
target_include_directories(airco_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/inc)
target_link_libraries(airco_core PUBLIC Threads::Threads)
if(NOT MSVC)
    target_compile_options(airco_core PRIVATE -Wall)
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
