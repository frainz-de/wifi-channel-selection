#pragma once

#include <thread>
#include <string>

class NeighborManager {
public:
    NeighborManager(const std::string& interface);
    void run(volatile bool* running);
    std::thread start_thread(volatile bool* running);
    
private:
    std::string interface; 
};
