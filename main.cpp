extern "C" {
    #include "spectral_common.h"
}
#include <iostream>
#include <fstream>
#include <cstring>
#include <stdio.h>
#include <unistd.h>
#include <chrono>
#include <iomanip>
#include <thread>
#include <signal.h>
#include <filesystem>
#include <algorithm>
#include <dirent.h>
//#include <Eigen/Dense>
#include "sample.h"
#include "collector.h"

using DataPoint = std::tuple<int, double>;
using TxDataPoint = std::tuple<std::chrono::milliseconds, long>;

// global variable to exit main loop gracefully
volatile bool running = true;

static inline void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}


// execute system command and return stdout as string
std::string exec(const std::string cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

// read a sample from proc, convert it to an object and store it in a vector
fft_sample_ath10k* readSample(std::ifstream &scanfile, std::vector<Sample*> &received_series) {

    scanfile.peek(); // check for EOF
    if(scanfile.eof()) {
        throw std::runtime_error("EOF reached");
    }

    auto sample = new fft_sample_ath10k;
    scanfile.read((char*)&sample->tlv, sizeof(fft_sample_tlv)); //read TLV header
//    std::cout << "type: " << unsigned(sample->tlv.type) << std::endl;
//    std::cout << "length: " << be16toh(sample->tlv.length) << std::endl;
    if(sample->tlv.type != ATH_FFT_SAMPLE_ATH10K) {
        throw std::runtime_error("Wrong sample type, only ath10k samples are supportet atm\n");
    }

    // read rest of header
    scanfile.read((char*)sample + sizeof(fft_sample_tlv), sizeof(fft_sample_ath10k) - sizeof(fft_sample_tlv));

    // create buffer and read in the FFT bins
    auto datalength = be16toh(sample->tlv.length) - sizeof(fft_sample_ath10k) + sizeof(fft_sample_tlv);
    auto readSample = new Sample(datalength);

//    auto data = new uint8_t[datalength];
    scanfile.read((char*) readSample->data, datalength);

    // calculate signal strength
    int squaresum = 0;
    for (decltype(datalength) i = 0; i < datalength; i++) {
        int value = readSample->data[i] << sample->max_exp;
        squaresum += (value*value);
    }
    //float power = sample->noise + sample->rssi + 20 *

    // fill Sample object
    readSample->rssi = sample->rssi;
    readSample->noise = be16toh(sample->noise);
    readSample->center_freq = be16toh(sample->freq1);

    received_series.push_back(readSample);

    scanfile.peek(); //set EOF bit if no data available
    return sample;
}

// catch SIGINT to terminate gracefully
void signalHandler(int sig) {
    if(sig == SIGINT) {
        running = false;
        signal(SIGINT, SIG_DFL);
    }
}

void manage_neighbors(const std::string& interface) {
    signal(SIGINT, signalHandler);
    std::cout << "\n starting scan\n";
    std::string neighbors;
    neighbors = exec("for i in $(iw dev " + interface + " scan -u | grep '42:42:42' |  awk '{ s = \"\"; for (i = 6; i <= NF; i++) s = s $i; print s }'); do echo $i | xxd -p -r; printf '\n'; done | sort");
    std::cout << std::endl << neighbors << std::endl;

    std::cout << "\n scan finished\n" << std::flush;

    // get neighbors regularly, not working yet
    /*
    std::chrono::time_point<std::chrono::system_clock> lastrun = 
        std::chrono::system_clock::now() - std::chrono::seconds(60);;
    while (running) {
        if ((lastrun - std::chrono::system_clock::now()) > std::chrono::seconds(60)) {
            neighbors = exec("for i in $(iw dev " + interface + " scan -u | grep '42:42:42' |  awk '{ s = \"\"; for (i = 6; i <= NF; i++) s = s $i; print s }'); do echo $i | xxd -p -r; printf '\n'; done | sort");
        std::cout << std::endl << neighbors << std::endl;
        lastrun = std::chrono::system_clock::now();
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));

    }
    */
}

int main(int argc, char* argv[]) {
    std::string interface = "wlp5s0";
    std::string phy;// = "phy1";
    std::string txpath;
    std::string scanpath;
    int channel = 0;

    // hacky argument handling
    if(argc > 1) {
        interface = argv[1];
    }

    if(argc > 2) {
        channel = std::stoi(argv[2]);
    }

    if(channel) {
        std::string mode = channel < 5000 ? "HT20" : "80MHz";
        std::system(("/usr/bin/iw dev " + interface + " set freq " + std::to_string(channel) + " " + mode).c_str());
    }

    std::thread neighbor_thread(manage_neighbors, interface);
    
    Collector collector(interface);
    std::thread collector_thread = collector.start_thread(&running);
    
    neighbor_thread.join();
    collector_thread.join();

    return 0;
}
