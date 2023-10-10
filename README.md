# CPPND: Capstone Airconditioner Project

This is a the Capstone project in the [Udacity C++ Nanodegree Program](https://www.udacity.com/course/c-plus-plus-nanodegree--nd213).
 
This idea of this project is to have an airconditioning system simulator. 
The biggest thing in this project is that the temperature readings come from a Temperature Sensor (handled by a message queue) that can be simulated. The idea behind this was that you get the temperature readings from somewhere external (via wireless, i2c, spi,...) and that it can be handled in a similar way.

A number of threads are handling things :
* a thread handling the simulation of the temperature nodes
* a thread handling possible events
* a thread handling the states of the airconditioner itself

I have also implemented an observer pattern so that the temperaturenode somewhat simulates the wanted temperature (please note that this is not 100% accurate, but for this purpose i think it will do).

You can have 3 different modes : auto, manual heating and manual cooling.

In this project i have used QT for a very simple GUI just to get myself familiarized a bit with this framework. I have tried to not let the QT code interfere with the actual temperature and airco logic by using a third class that acts as the mediator between changes in the data  and QT.


## Dependencies for Running Locally
* cmake >= 3.7
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
  
## Basic Build Instructions

1. Clone this repo.
2. Make a build directory in the top level directory: `mkdir build && cd build`
3. Compile: `cmake -DCMAKE_PREFIX_PATH="/path/to/Qt/6.5.2/gcc_64" .. && make`
4. Run it: `./Airco`.

## Usage
* AUTO mode
- Turn on the Airco
- turn on the temperature simulation (this will increase or decrease to the target temperature)
- changing the target temperature will modify the state to heating or cooling 
- if the target temperature is reached, the Airco will go to Standby

* HEATING mode
This will only heat when required
Note : in this mode the temperature simulation will still go towards the set target temperature

* COOLING mode
This will only cool when required
Note : in this mode the temperature simulation will still go towards the set target temperature

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
- condition_variable : see Airco.cpp


