#include "Node.h"
#include <algorithm>
#include <iostream>
Node::~Node(){
    if (_running && _simulationThread != nullptr)
    {
        _running = false;
        _simulationThread->join();
    }
    std::cout << "~Node() called" << std::endl;

}