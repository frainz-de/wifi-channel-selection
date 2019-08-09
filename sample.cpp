#include "sample.h"

Sample::Sample(size_t datalength) {
    /*
    timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch());
            */
    timestamp = std::chrono::system_clock::now();
    data = new uint8_t[datalength]; 
}

Sample::~Sample() {
    delete data;
}

void Sample::output(std::ostream &stream) {
    //long timecount = timestamp.count();
    long timecount = std::chrono::duration_cast<std::chrono::milliseconds>(timestamp.time_since_epoch()).count();
    stream << timecount << "," << unsigned(rssi) << "," << unsigned(center_freq) << ";\n";
}
