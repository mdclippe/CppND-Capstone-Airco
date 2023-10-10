#ifndef NODE_H
#define NODE_H
#include <iostream>
#include <thread>
#include <atomic>
#include "MessageQueue.h"

class Node {
public:
    explicit Node(std::shared_ptr<MessageQueue<int>> messageQueue, int id) : _messageQueue(messageQueue), _id(id)
    {
        _value = 0;
    };
    ~Node();
    
    int getID() {return _id;}
    void setName(const std::string& name) { this->_name = name;}
    std::string getName() {
        return this->_name;
    }
    void setValue(int val) {this->_value = val;}
    int getValue() { return this->_value;}
    virtual void startSimulate() = 0;
    virtual void stopSimulate() = 0;
protected:
    std::shared_ptr<MessageQueue<int>> _messageQueue;
    std::atomic<bool> _running{false};
    std::unique_ptr<std::thread> _simulationThread;

private:
    std::string _name;
    int _value;
    int _id;
    
};

#endif