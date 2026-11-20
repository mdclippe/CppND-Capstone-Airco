# CPPND: Capstone Airconditioner Project

This is a the Capstone project in the [Udacity C++ Nanodegree Program](https://www.udacity.com/course/c-plus-plus-nanodegree--nd213).
 
This idea of this project is to have an airconditioning system simulator. 
The biggest thing in this project is that the temperature readings come from a Temperature Sensor (handled by a message queue) that can be simulated. The idea behind this was that you get the temperature readings from somewhere external (via wireless, i2c, spi,...) and that it can be handled in a similar way.

A number of threads are handling things :
* a thread handling the simulation of the temperature nodes
* a thread handling possible events
* a thread handling the states of the airconditioner itself

I have also implemented an observer pattern so that the temperaturenode simulates the room : it observes the status of the airco and warms up while the airco is heating, cools down while it is cooling and keeps its temperature otherwise.

You can have 3 different modes : auto, manual heating and manual cooling.

In this project i have used QT for a very simple GUI just to get myself familiarized a bit with this framework. I have tried to not let the QT code interfere with the actual temperature and airco logic by using a third class that acts as the mediator between changes in the data  and QT.


## Dependencies for Running Locally
* cmake >= 3.16
  * All OSes: [click here for installation instructions](https://cmake.org/install/)
* make >= 4.1 (Linux, Mac), 3.81 (Windows)
  * Linux: make is installed by default on most Linux distros
  * Mac: [install Xcode command line tools to get make](https://developer.apple.com/xcode/features/)
  * Windows: [Click here for installation instructions](http://gnuwin32.sourceforge.net/packages/make.htm)
* gcc/g++ >= 5.4
  * Linux: gcc / g++ is installed by default on most Linux distros
  * Mac: same deal as make - [install Xcode command line tools](https://developer.apple.com/xcode/features/)
  * Windows: recommend using [MinGW](http://www.mingw.org/)
* Qt 6.5.2
  * used the binary installer : https://www.qt.io/download-qt-installer-oss?hsCtaTracking=99d9dd4f-5681-48d2-b096-470725510d34%7C074ddad0-fdef-4e53-8aa8-5e8a876d6ab4
  * The location of the Qt/6.5.2/gcc_64 must be passed to cmake, either via commandline or via the cmakelists.txt
## Basic Build Instructions

1. Clone this repo.
2. Make a build directory in the top level directory: `mkdir build && cd build`
3. Compile: `cmake -DCMAKE_PREFIX_PATH="/path/to/Qt/6.5.2/gcc_64" .. && make`
4. Run it: `./Airco`.

CMake options:

* `-DAIRCO_BUILD_GUI=OFF` : build without Qt (only the airco logic and the tests)
* `-DAIRCO_BUILD_TESTS=OFF` : do not build the tests (GoogleTest is downloaded at configure time)
* `-DAIRCO_WITH_MQTT=OFF` : build without MQTT support (see below)

## MQTT

The airco can be connected to an MQTT broker (e.g. [Mosquitto](https://mosquitto.org/)) with the
[Eclipse Paho MQTT C++](https://github.com/eclipse-paho/paho.mqtt.cpp) library.
If Paho is installed (Ubuntu: `sudo apt install libpaho-mqttpp-dev`) CMake uses it, otherwise
Paho C++ v1.6.0 and the Paho C library are downloaded and built together with the project.

At start-up the application connects to the broker in `AIRCO_MQTT_BROKER` (default `tcp://localhost:1883`).
Without a reachable broker it simply runs without MQTT.
The topics start with `AIRCO_MQTT_TOPIC_PREFIX` (default `airco`):

| Topic | Direction | Payload |
| --- | --- | --- |
| `airco/sensor/temperature` | in | a temperature reading, e.g. `21` or `21.4` (rounded); other payloads are ignored |
| `airco/status` | out, retained | `OFF`, `STANDBY`, `HEATING` or `COOLING` |
| `airco/target_temperature` | out, retained | the target temperature, e.g. `22` |
| `airco/current_temperature` | out, retained | the last reading used by the airco |

Readings from MQTT go to the same message queue as the simulated `TemperatureNode`, so any sensor on the network can drive the airco.
`AircoMqttBridge` does this in the same way `Monitor` connects the airco to Qt: it is an `AircoObserver`.
It only knows the small `MqttClient` interface; `PahoMqttClient` implements it with Paho, which keeps the airco logic testable without a broker.

Try it with a local broker:

```sh
docker run -it -p 1883:1883 eclipse-mosquitto mosquitto -c /mosquitto-no-auth.conf
mosquitto_sub -t 'airco/#' -v                         # follow the airco
mosquitto_pub -t airco/sensor/temperature -m 17.5     # send a temperature reading
```

## Testing

The tests use [GoogleTest / GoogleMock](https://github.com/google/googletest) (v1.15.2, fetched by CMake) and live in `tests/`:

| Folder | Executable | What is tested |
| --- | --- | --- |
| `tests/unit` | `airco_unit_tests` | one class at a time: `MessageQueue`, `Node`, `TemperatureNode`, `Airco` (with mocked observers), `AircoMqttBridge` (with a mocked MQTT client) and `Monitor` (Qt only) |
| `tests/integration` | `airco_integration_tests` | `Airco` + `TemperatureNode` + `MessageQueue` working together (fast simulation step), `Airco` + `AircoMqttBridge` with an in-memory broker, `PahoMqttClient` (MQTT only), `Airco` -> `Monitor` Qt signals (Qt only) |
| `tests/system` | `airco_system_tests` | the complete application wired like `main.cpp` with production timing, driven through user scenarios, through a real MQTT broker (MQTT only) and through the real `MainWindow` buttons and slider (Qt only) |

Build and run all tests:

```sh
mkdir build && cd build
cmake -DCMAKE_PREFIX_PATH="/path/to/Qt/6.5.2/gcc_64" ..   # or: cmake -DAIRCO_BUILD_GUI=OFF ..
make
ctest --output-on-failure
```

Or with the CMake presets (build directory `build/<preset>`):

```sh
cmake --preset no-gui            # Windows + MSVC: msvc-no-gui, with Qt: default
cmake --build --preset no-gui
ctest --preset no-gui
```

For your own Qt location, add a `CMakeUserPresets.json` (not committed), e.g.:

```json
{
    "version": 3,
    "configurePresets": [
        { "name": "my-qt", "inherits": "default",
          "cacheVariables": { "CMAKE_PREFIX_PATH": "/home/me/Qt/6.5.2/gcc_64" } }
    ]
}
```

On Windows the Qt online installer installs Qt for MinGW, which needs the MinGW compiler that comes with it
(MSVC cannot use those Qt libraries). A matching `CMakeUserPresets.json` preset:

```json
{ "name": "mingw-qt", "inherits": "default", "generator": "Ninja",
  "cmakeExecutable": "C:/Qt/Tools/CMake_64/bin/cmake.exe",
  "environment": { "PATH": "C:/Qt/Tools/mingw1310_64/bin;C:/Qt/Tools/Ninja;$penv{PATH}" },
  "cacheVariables": { "CMAKE_PREFIX_PATH": "C:/Qt/6.12.0/mingw_64",
                      "CMAKE_C_COMPILER": "gcc", "CMAKE_CXX_COMPILER": "g++" } }
```

On Windows the build copies the Qt DLLs next to the executables (`windeployqt`), so the app and the tests
also start outside Qt Creator.

In VS Code (CMake Tools + C++ TestMate): choose a configure preset when CMake Tools asks
(or `CMake: Select Configure Preset`), build with `CMake: Build` (F7) and the tests
appear in the Testing view, where they can be run and debugged one by one.

Run one category: `ctest -L unit`, `ctest -L integration` or `ctest -L system`.
The executables can also be started directly, e.g. `./tests/airco_unit_tests --gtest_filter=AircoTest.*`.

The system tests use the real one-degree-per-second simulation, so they take a few seconds each.
The GUI system tests run on the Qt `offscreen` platform on Linux/macOS (override with `QT_QPA_PLATFORM`).
`TemperatureNode` takes an optional step interval (default 1 second) so the unit and integration tests can simulate faster.
The tests that need a real MQTT broker use `AIRCO_TEST_MQTT_BROKER` (default `tcp://localhost:1883`) and are reported as skipped when no broker is reachable.

## Usage
* AUTO mode
- Turn on the Airco
- turn on the temperature simulation (the room warms up one degree per second while the Airco is heating, cools down while it is cooling and keeps its temperature in standby or when the Airco is off)
- changing the target temperature will modify the state to heating or cooling 
- if the target temperature is reached, the Airco will go to Standby

* HEATING mode
This will only heat when required : with a target below the room temperature the Airco stays in standby and the simulated room keeps its temperature

* COOLING mode
This will only cool when required : with a target above the room temperature the Airco stays in standby and the simulated room keeps its temperature

## Rubric Points
1. Object oriented programming

- Object Oriented programming : see Airco.h
- Monitor and temperaturenode inherit from virtual AircoObserver class 
- Templates : see MessageQueue.cpp

2. Memory management
- Project uses references : see Mainwindow.cpp constructor
- Destructors : implemented throughout and these are responsible for stopping threads.
- move semantcos : see MessageQueue.cpp and TemperatureNode.cpp (30);
- smart pointers : shared pointers and unique pointers are used where possible

3. Concurrency
- multithreaded : TemperatureNode has a simulation thread, Airco has an eventhandler thread and a temperature receiving thread
- locking : Airco contains the lock_guard mechanism the protect the variable, same as MessageQueue.cpp
- condition_variable : see MessageQueue.cpp


