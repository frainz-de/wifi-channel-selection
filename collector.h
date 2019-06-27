#pragma once

#include <string>
#include <fstream>
#include <vector>
#include "sample.h"
#include <chrono>
#include <tuple>
#include <thread>

using DataPoint = std::tuple<int, double>;
using TxDataPoint = std::tuple<std::chrono::milliseconds, long>;

class Collector {
public:
    Collector(std::string& interface);
    void run(volatile bool* running);

    std::thread start_thread(volatile bool* running);
    fft_sample_ath10k* readSample(std::ifstream &scanfile, std::vector<Sample*> &received_series);
private:

    std::string interface;
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
