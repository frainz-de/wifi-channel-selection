#pragma once

#include "neighbor.h"
#include <thread>
#include <string>
#include <set>
#include <map>

class NeighborManager {
public:
    NeighborManager(const std::string& interface);
    void run(volatile bool* running, int abortpipe);
    void scanandsend();

    std::thread start_thread(volatile bool* running, int abortpipe);
    
private:
    void scan();
    void send_neighbors();
    void send_msg(const std::string address, const std::string msg);

    std::string interface; 
    std::string own_address;
    int own_channel;

    //TODO make access to this thread safe
    std::set<std::string> neighbors;
    //TODO store as pairs of address and channel, maybe own class?
    std::set<std::string> neighbors_neighbors;

    std::map<std::string, int> channels;
};
