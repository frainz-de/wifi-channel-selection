#include <chrono>

class Sample {
  public:
    Sample(size_t datalength);

      std::chrono::milliseconds timestamp;
    float center_freq;
    uint8_t rssi;
    uint8_t* data;
    int16_t noise;

};
