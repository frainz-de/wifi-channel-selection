#pragma once

//#include "neighbor_manager.h"
#include "sample.h"

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
class NeighborManager;

class Collector {
private:
    void seek_to_header();

    NeighborManager* neighbor_manager;

    std::string specinterface;
    std::string netinterface;
    std::string phy; // = "phy1";
    std::string txpath;
    std::string scanpath;
    std::ifstream scanfile;
    std::ifstream txfile;
    // diagnostics data files
    std::ofstream outputscanfile = std::ofstream("specdata.csv");
    std::ofstream outputtxfile = std::ofstream("txdata.csv");
    std::mutex file_lock;

    //TODO make both lists (+required changes) to not invalidate iterators
    //std::vector<Sample*> received_series;
    std::list<Sample*> received_series;
    //std::deque<Sample*> received_series;
    //std::vector<TxDataPoint> tx_series;
    //std::deque<TxDataPoint> tx_series;
    std::list<TxDataPoint> tx_series;

    long last_tx_bytes;
    int sample_count = 0;
    float avg_rssi = 0;

public:
    Collector(std::string& specinterface, std::string& netinterface);
    void run(volatile bool* running);

    std::thread start_thread(volatile bool* running);
    void readSample(std::ifstream &scanfile, decltype(received_series) &received_series);
    void set_neighbor_manager(NeighborManager* neighbor_manager);

    nlohmann::json get_tx(size_t max_size);
    double correlate(const std::vector<double>& txvector, long timeint);

    void truncate(std::chrono::milliseconds time);
};
