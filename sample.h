#include <chrono>

class Sample {
  public:
    Sample();

      std::chrono::milliseconds timestamp;
    float center_freq;
    float noise;
    uint8_t rssi;

};
