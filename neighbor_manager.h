#pragma once

#include <thread>
#include <string>
#include <set>

class NeighborManager {
public:
    NeighborManager(const std::string& interface);
    void run(volatile bool* running, int abortpipe);
    std::thread start_thread(volatile bool* running, int abortpipe);
    void scanandsend();
    
private:
    void scan();
    void send_neighbors();

    std::string interface; 
    std::string own_address;
    int own_channel;

    //TODO make access to this thread safe
    std::set<std::string> neighbors;
    std::set<std::string> neighbors_neighbors;
};
