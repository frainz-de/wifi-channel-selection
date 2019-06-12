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
#include "dirent.h"
//#include <Eigen/Dense>
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

void signalHandler(int sig) {
    if(sig == SIGINT) {
        running = false;
        signal(SIGINT, SIG_DFL);
    }
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

    { // clean up the c junk after deducing the interface
        auto dir = opendir(("/sys/class/net/" + interface + "/device/ieee80211/").c_str()); 
        if (!dir) {
            std::cerr << "Error deducing interface: " << strerror(errno) << std::endl;
            return 1;
        }
        readdir(dir);
        readdir(dir);
        auto ent = readdir(dir);
        closedir(dir);
        phy = ent->d_name;
        txpath = "/sys/class/net/" + interface + "/statistics/tx_bytes";
        scanpath = "/sys/kernel/debug/ieee80211/" + phy + "/ath10k/spectral_scan0";
    }

    if(channel) {
        std::string mode = channel < 5000 ? "HT20" : "80MHz";
        std::system(("/usr/bin/iw dev " + interface + " set freq " + std::to_string(channel) + " " + mode).c_str());
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
    int sample_count = 0;
    float avg_rssi = 0;

    while (running) {
        // read available samples
        scanfile.peek();
        while(!scanfile.eof()) {
            auto readsample = readSample(scanfile, received_series);
            sample_count++;
            delete readsample; // looks like we don't actually need it here
        }

        //TODO running average of rssi
        if(sample_count > 1) {
            auto previous = received_series[received_series.size()-2];
            auto current = received_series[received_series.size()-1];
            auto delta_t = current->timestamp - previous->timestamp;
            std::chrono::milliseconds tau(1000);
            //double alpha = delta_t / tau;
            double alpha = 0.1;
            //std::cout << alpha << std::endl;
            //avg_rssi += alpha * (current->rssi - avg_rssi);
            avg_rssi = (1-alpha)*avg_rssi + alpha*current->rssi;
            //avg_rssi = current->rssi;
        }

        // get current time
        auto now = std::chrono::high_resolution_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        //std::cout << std::ctime(&in_time_t) << std::endl;
        //std::cout << std::flush << "\r" << std::ctime(&in_time_t) << "\r" << std::flush;
        std::string time = std::ctime(&in_time_t);
        rtrim(time);
        std::cout << "\r" << time << ": collected " << sample_count << " samples, rssi: " << avg_rssi << std::flush;

        // fill tx statistics vector
        txfile.seekg(0); // seek to the beginning to get a new value
        std::string tx_bytes_string;
        std::getline(txfile, tx_bytes_string);
        long tx_bytes = std::stol(tx_bytes_string); // convert string to long
        TxDataPoint txdatapoint(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()),
                tx_bytes-last_tx_bytes);
        last_tx_bytes = tx_bytes;
        tx_series.push_back(txdatapoint);

        //sleep(1);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
//        std::cout << "waiting\n";
        scanfile.clear();
        //running = false;
    }

    std::cout << "\ncaught signal\n";

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
