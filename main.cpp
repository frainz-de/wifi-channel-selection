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
#include <signal.h>
#include <Eigen/Dense>
#include "sample.h"

using DataPoint = std::tuple<int, double>;
using TxDataPoint = std::tuple<std::chrono::milliseconds, long>;

bool running = true;

static inline void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}

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

void signalHandler(int signal) {
    if(signal == SIGINT) {
        running = false;
    }
}

int main(int argc, char* argv[]) {
    std::string scanpath = "/sys/kernel/debug/ieee80211/phy1/ath10k/spectral_scan0";
    std::string txpath = "/sys/class/net/wlp5s0/statistics/tx_bytes";

    // hacky argument handling
    if(argc > 1) {
        scanpath = argv[1];
    }

    signal(SIGINT, signalHandler);

    // try to open (virtual) file in binary mode
    std::ifstream scanfile;
    scanfile.open(scanpath ,std::fstream::in | std::fstream::binary);
    if(scanfile.fail()) {
        std::cerr << "Failed to open scan file: " << strerror(errno) << std::endl;
        return 1;
    }

    // try to open network statistics file
    std::ifstream txfile;
    txfile.open(txpath, std::fstream::in);
    if(txfile.fail()) {
        std::cerr << "Failed to open network statistics file: " << strerror(errno) << std::endl;
        return 1;
    }

    //remove characters until valid header is found
    if(scanfile.peek() != ATH_FFT_SAMPLE_ATH10K) {
        std::cerr << "Invalid header, discarding until valid\n";
        while(scanfile.peek() != ATH_FFT_SAMPLE_ATH10K) {
            scanfile.ignore(1);
        }
    }

    std::vector<Sample*> received_series;
    std::vector<TxDataPoint> tx_series;
    long last_tx_bytes;

    while (running) {
        // read available samples
        scanfile.peek();
        while(!scanfile.eof()) {
            auto sample = readSample(scanfile, received_series);
            delete sample; // looks like we don't actually need it here
        }

        // get current time
        auto now = std::chrono::high_resolution_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        //std::cout << std::ctime(&in_time_t) << std::endl;
        //std::cout << std::flush << "\r" << std::ctime(&in_time_t) << "\r" << std::flush;
        std::string time = std::ctime(&in_time_t);
        rtrim(time);
        std::cout << std::flush << "\r" << time << std::flush;

        // fill tx statistics vector
        txfile.seekg(0);
        std::string tx_bytes_string;
        std::getline(txfile, tx_bytes_string);
        long tx_bytes = std::stol(tx_bytes_string);
        TxDataPoint txdatapoint(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()),
                tx_bytes-last_tx_bytes);
        last_tx_bytes = tx_bytes;
        tx_series.push_back(txdatapoint);

        sleep(.1);
//        std::cout << "waiting\n";
        scanfile.clear();
        //running = false;
    }

    std::cout << "caught signal\n";
    // output scan data
    std::ofstream outputscanfile("specdata.csv");
    for (auto const& sample: received_series) {
        sample->output(outputscanfile);
    }
    outputscanfile.close();

    // output tx data
    std::ofstream outputtxfile("txdata.csv");
    for (auto const& datapoint: tx_series) {
        outputtxfile << std::get<0>(datapoint).count() << ";" << std::get<1>(datapoint) << ";\n";
    }

    // scanfile.close(); // the destructor does this for us
    
    return 0;
}
