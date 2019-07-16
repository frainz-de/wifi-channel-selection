extern "C" {
    #include "spectral_common.h"
}
#include "collector.h"
#include "helpers.h"

#include <iostream>
#include <string>
#include <dirent.h>
#include <cstring>
#include <thread>
#include <algorithm>
#include <cassert>
#include <sstream>

Collector::Collector(std::string& interface) {
    
    { // clean up the c junk after deducing the interface
        auto dir = opendir(("/sys/class/net/" + interface + "/device/ieee80211/").c_str()); 
        if (!dir) {
            std::cerr << "Error deducing interface: " << strerror(errno) << std::endl;
            throw std::runtime_error("");

        }
        readdir(dir);
        readdir(dir);
        auto ent = readdir(dir);
        closedir(dir);
        phy = ent->d_name;
        txpath = "/sys/class/net/" + interface + "/statistics/tx_bytes";
        scanpath = "/sys/kernel/debug/ieee80211/" + phy + "/ath10k/spectral_scan0";
    }


    // try to open (virtual) file in binary mode
    scanfile.open(scanpath ,std::fstream::in | std::fstream::binary);
    if(scanfile.fail()) {
        std::cerr << "Failed to open scan file: " << strerror(errno) << std::endl;
        throw std::runtime_error("");
    }
}

std::thread Collector::start_thread(volatile bool* running) {
    std::thread collector_thread(&Collector::run, this, running);
    return collector_thread;
}

//remove characters until valid header is found
void Collector::seek_to_header() {
    if(scanfile.peek() != ATH_FFT_SAMPLE_ATH10K) {
        std::cerr << "Invalid header, discarding until valid\n";
        while(scanfile.peek() != ATH_FFT_SAMPLE_ATH10K && !scanfile.eof()) {
            scanfile.ignore(1);
        }
    }
    std::cout << "seeking to header\n";

}

void Collector::run(volatile bool* running) {

    seek_to_header();

    while (*running) {
        // read available samples
        scanfile.peek();
        while(!scanfile.eof()) {
            readSample(scanfile, received_series);
            sample_count++;
        }

        //TODO running average of rssi
        if(sample_count > 1) {
            auto previous = received_series.at(received_series.size()-2);
            auto current = received_series.at(received_series.size()-1);
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
        std::string time = std::ctime(&in_time_t);
        rtrim(time);

        // print status
        std::stringstream output;
        output << "\r" << time << ": collected " << sample_count
            << " samples, rssi: " << avg_rssi << "    " << std::flush;
        std::cout << output.str();
        
        // try to open network statistics file
        std::ifstream txfile;
        txfile.open(txpath, std::fstream::in);
        if(txfile.fail()) {
            std::cerr << "Failed to open network statistics file: " << strerror(errno) << std::endl;
            throw std::runtime_error("");
        }
        
        /*
        // fill tx statistics vector
        // txfile.seekg(0, std::ios_base::beg); // seek to the beginning to get a new value
        txfile.seekg(0); // seek to the beginning to get a new value
        if(txfile.fail()) {
            std::cerr << "\nFailed reading network statistics: " << strerror(errno) << std::endl;
        }
        */
        //std::string tx_bytes_string;
        //std::getline(txfile, tx_bytes_string);
        //txfile >> tx_bytes_string;
        //std::getline(txfile, tx_bytes_string);
        //long tx_bytes = std::stol(tx_bytes_string); // convert string to long
        long tx_bytes;
        txfile >> tx_bytes;
        TxDataPoint txdatapoint(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()),
                tx_bytes-last_tx_bytes);
        last_tx_bytes = tx_bytes;
        tx_series.push_back(txdatapoint);

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
//        std::cout << "waiting\n";
        scanfile.clear();
        //running = false;
    }

    std::cout << "\ncaught SIGINT, writing data to disc\n";

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
    std::cout << "finished writing, exiting\n";
}

// read a sample from proc, convert it to an object and store it in a vector
void Collector::readSample(std::ifstream &scanfile, std::vector<Sample*> &received_series) {

    scanfile.peek(); // check for EOF
    if(scanfile.eof()) {
        throw std::runtime_error("EOF reached");
    }

    auto sample = new fft_sample_ath10k;
    scanfile.read((char*)&sample->tlv, sizeof(fft_sample_tlv)); //read TLV header
//    std::cout << "type: " << unsigned(sample->tlv.type) << std::endl;
//    std::cout << "length: " << be16toh(sample->tlv.length) << std::endl;
    if(sample->tlv.type != ATH_FFT_SAMPLE_ATH10K) {
        //throw std::runtime_error("Wrong sample type, only ath10k samples are supportet atm\n");
        seek_to_header();
        return;
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
    delete sample;
}
