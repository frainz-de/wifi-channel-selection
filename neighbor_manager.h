#pragma once

#include "neighbor.h"
//#include "collector.h"
#include <thread>
#include <string>
#include <set>
#include <map>
#include <mutex>
#include <nlohmann/json.hpp>

class Collector;
class ChannelStrategy;
//enum StrategyType {RANDOM, CORRELATION};

class NeighborManager {
public:
    NeighborManager(const std::string& specinterface, const std::string& netinterface,
            const std::string& strategytype);
    void run(volatile bool* running, int abortpipe);
    void scanandsend();
    void send_tx();
    void set_collector(Collector* collector);
    //int get_freq_from_neighbor(std::string address);

    std::thread start_thread(volatile bool* running, int abortpipe);
    std::set<std::string> get_neighbors();
    std::set<std::string> get_direct_neighbors();
    std::string get_own_address();
    void print_neighbors();
    
private:
    void scan();
    void send_neighbors();
    void send_msg(const std::string address, const std::string msg);
    void receive_message(int sockfd);

    Collector* collector;
    ChannelStrategy* channel_strategy;

    std::mutex send_lock;

    std::string specinterface;
    std::string netinterface;
    std::string own_address;

    //TODO make access to this thread safe
    std::set<std::string> neighbors;
    //TODO store as pairs of address and channel, maybe own class?
    std::set<std::string> neighbors_neighbors;

    //std::map<std::string, int> channels;
    //std::map<std::string, double> correlations;
};
