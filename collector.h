#pragma once

//#include "neighbor_manager.h"
#include "sample.h"
#include "helpers.h"

#include <string>
#include <fstream>
#include <vector>
#include <list>
#include <queue>
#include <chrono>
#include <tuple>
#include <thread>
#include <mutex>
#include <nlohmann/json.hpp>

using DataPoint = std::tuple<int, double>;
using TxDataPoint = std::tuple<std::chrono::time_point<std::chrono::high_resolution_clock>, long>; // timestamp, value
using Clock = std::chrono::high_resolution_clock;
class NeighborManager;

class Collector {
private:
    void seek_to_header();

    NeighborManager* neighbor_manager;

    std::string specinterface;
    std::string netinterface;
    std::string phy;
    std::string txpath;
    std::string rxpath;
    std::string scanpath;
    std::ifstream scanfile;

    // diagnostics data files
    std::ofstream outputscanfile = fileoutput ? std::ofstream("specdata.csv") : std::ofstream("/dev/null");
    std::ofstream outputtxfile = fileoutput ? std::ofstream("txdata.csv") : std::ofstream("/dev/null");
    std::mutex file_lock;

    std::list<Sample*> received_series;
    std::list<TxDataPoint> tx_series;

    long last_net_bytes;
    int sample_count = 0;
    float avg_rssi = 0;
    int net_count = 0;

public:
    Collector(std::string& specinterface, std::string& netinterface);
    void run(volatile bool* running);

    std::thread start_thread(volatile bool* running);
    void readSample(std::ifstream &scanfile, decltype(received_series) &received_series);
    void set_neighbor_manager(NeighborManager* neighbor_manager);

    nlohmann::json get_tx(size_t max_size);
    double correlate(const std::vector<double>& txvector, long timeint);
    std::tuple<int, double, std::chrono::time_point<Clock>> get_rx_power(std::chrono::milliseconds duration);

    void truncate(std::chrono::milliseconds time);
};
