#pragma once

#include <thread>
#include <string>
#include <set>

class NeighborManager {
public:
    NeighborManager(const std::string& interface);
    void run(volatile bool* running);
    std::thread start_thread(volatile bool* running);
    
private:
    void scan();
    void send_neighbors();

    std::string interface; 
    std::string own_address;
    std::set<std::string> neighbors;
    std::set<std::string> neighbors_neighbors;
};
