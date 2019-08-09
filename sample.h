#pragma once

#include <chrono>
#include <iostream>

class Sample {
  public:
    Sample(size_t datalength);
    ~Sample();
    void output(std::ostream &stream);

    std::chrono::time_point<std::chrono::high_resolution_clock> timestamp;
    float center_freq;
    uint8_t rssi;
    uint8_t* data;
    int16_t noise;

};
