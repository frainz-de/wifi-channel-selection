#include "sample.h"

Sample::Sample(size_t datalength) {
    timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch());
    data = new uint8_t[datalength]; 
}
