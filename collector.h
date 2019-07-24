#pragma once

//#include "neighbor_manager.h"

#include <string>
#include <fstream>
#include <vector>
#include "sample.h"
#include <chrono>
#include <tuple>
#include <thread>
#include <nlohmann/json.hpp>

using DataPoint = std::tuple<int, double>;
using TxDataPoint = std::tuple<std::chrono::milliseconds, long>;
class NeighborManager;

class Collector {
public:
    Collector(std::string& specinterface, std::string& netinterface);
    void run(volatile bool* running);

    std::thread start_thread(volatile bool* running);
    void readSample(std::ifstream &scanfile, std::vector<Sample*> &received_series);
    void set_neighbor_manager(NeighborManager* neighbor_manager);

    nlohmann::json get_tx(size_t max_size);

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

    std::vector<Sample*> received_series;
    std::vector<TxDataPoint> tx_series;
    long last_tx_bytes;
    int sample_count = 0;
    float avg_rssi = 0;
};
