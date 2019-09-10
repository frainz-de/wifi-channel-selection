extern "C" {
    #include "spectral_common.h"
}
#include "collector.h"
#include "helpers.h"
#include "neighbor_manager.h"

#include <iostream>
#include <string>
#include <dirent.h>
#include <cstring>
#include <thread>
#include <algorithm>
#include <cassert>
#include <sstream>
#include <nlohmann/json.hpp>
#include <signal.h>

Collector::Collector(std::string& specinterface, std::string& netinterface) {
    
    // deduce phy and spectral scan pathes from interface name
    {
        auto dir = opendir(("/sys/class/net/" + specinterface + "/device/ieee80211/").c_str()); 
        if (!dir) {
            std::cerr << "Error deducing specinterface: " << strerror(errno) << std::endl;
            throw std::runtime_error(std::string{"Error deducing specinterface: "} + strerror(errno));
        }
        readdir(dir);
        readdir(dir);
        auto ent = readdir(dir);
        closedir(dir);
        phy = ent->d_name;
        scanpath = "/sys/kernel/debug/ieee80211/" + phy + "/ath10k/spectral_scan0";
    }

    txpath = "/sys/class/net/" + netinterface + "/statistics/tx_bytes";
    rxpath = "/sys/class/net/" + netinterface + "/statistics/rx_bytes";

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

void Collector::set_neighbor_manager(NeighborManager* neighbor_manager) {
    this->neighbor_manager = neighbor_manager;
}

nlohmann::json Collector::get_tx(size_t max_size) {
    // write last second into json
    auto now = std::chrono::high_resolution_clock::now();
    auto current_time = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
    auto begin_time = current_time - std::chrono::seconds(1);
    nlohmann::json txmsg;
    auto txdata = nlohmann::json::array();

    //for (auto i = tx_series.size()-max_size; i < tx_series.size(); i++) {
    ////while(json.get_ref<std::string&>().size() < max_size) {
    //    txdata.push_back(std::get<1>(tx_series.at(i)));
    //    //i--;
    //}

    for (auto i = std::prev(tx_series.end(), max_size); i != tx_series.end(); i++) {
    //while(json.get_ref<std::string&>().size() < max_size) {
        txdata.push_back(std::get<1>(*i));
        //i--;
    }


    txmsg["txdata"] = txdata;
    txmsg["timestamp"] = current_time.count();
    return txmsg;
}

double Collector::correlate(const std::vector<double>& txvector, long timeint) {
    if(received_series.size() == 0) {
        std::cerr << "\ncannot correlate without data\n";
        return nan("");
    }
    auto tx_timestamp = std::chrono::time_point<std::chrono::high_resolution_clock>(std::chrono::milliseconds(timeint));
    auto index = --received_series.end();
    auto last = (*index)->timestamp;

    // search beginning of correlation interval
    for(; index != --received_series.begin() && (*index)->timestamp > tx_timestamp; --index);
    if (index == --received_series.begin()) {
        std::cerr << "\n\033[31mnot enough data for correlation\033[0m\n";
        return nan("");
    }

    assert (index != received_series.begin());
    auto ftimestamp = (*index)->timestamp;

    //auto tx_timestamp = timestamp;
    double rx_avg = 0;
    double tx_avg = 0;
    int interval = 0;
    auto rxindex = index;
    auto rxendindex = received_series.end();

    for(auto vindex = txvector.begin(); vindex != txvector.end(); ++vindex) {
        tx_timestamp += std::chrono::milliseconds(1);
        double avg_rssi = 0;
        int avgcounter = 0;
        //for(; (*findex)->timestamp < tx_timestamp; ++findex) {
        //TODO maybe correlate slightly earlier intervals
        while(rxindex != rxendindex &&  (*rxindex)->timestamp < tx_timestamp) {
            avg_rssi += (*rxindex)->rssi;
            ++avgcounter;
            ++rxindex;
            //assert((*findex));
            //assert(findex != received_series.end());
        }
        avg_rssi /= (double) avgcounter;
        tx_avg += *vindex;
        if(avgcounter) {
            rx_avg += avg_rssi;
        }
        interval++;
    }
    // TODO: use pearson correlation (normalize)
    rx_avg /= interval;
    tx_avg /= interval;


    double prodsum = 0;
    rxindex = index;
    for(auto vindex = txvector.begin(); vindex != txvector.end(); ++vindex) {
        tx_timestamp += std::chrono::milliseconds(1);
        double avg_rssi = 0;
        int avgcounter = 0;
        while(rxindex != rxendindex &&  (*rxindex)->timestamp < tx_timestamp) {
            avg_rssi += (*rxindex)->rssi;
            ++avgcounter;
            ++rxindex;
            //assert((*findex));
            //assert(findex != received_series.end());
        }
        avg_rssi /= (double) avgcounter;
        auto prod = (*vindex-tx_avg) * (avg_rssi-rx_avg);
        if(avgcounter) {
            prodsum += prod;
        }
        interval++;
    }

    auto e = prodsum / interval;
    return e;
}

// delete everything in the data series exept for the interval given by time
void Collector::truncate(std::chrono::milliseconds time) {
    std::chrono::time_point<std::chrono::high_resolution_clock> cuttime =
        std::chrono::high_resolution_clock::now() - time;

    std::lock_guard<std::mutex> guard(file_lock);

    for (auto i = received_series.begin(); received_series.size() > 2 && (*i)->timestamp < cuttime;) {
        (*i)->output(outputscanfile);
        delete *i;
        received_series.erase(i++);
    }

    for (auto i = tx_series.begin(); tx_series.size() > 2 && std::get<0>(*i) < cuttime;) {
        tx_series.erase(i++);
        outputtxfile << std::chrono::duration_cast<std::chrono::milliseconds>(std::get<0>(*i).time_since_epoch()).count()
            << ";" << std::get<1>(*i) << ";\n";
    }
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
            //auto& previous = received_series.at(received_series.size()-2);
            auto previous = *std::prev(received_series.end(), 2);
            //auto& current = received_series.at(received_series.size()-1);
            auto current = received_series.back();
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

        auto last_freq = received_series.back() ? received_series.back()->center_freq : 0;
        // print status
        if (verbosity >= 1) {
            std::cout << "\r" << time << ": collected " << sample_count
                << " samples, freq: " << last_freq << ", rssi: " << avg_rssi
                << " and " << net_count << " network samples"  << "    " << std::flush;
        }
        
        // try to open network statistics file
        std::ifstream txfile;
        txfile.open(txpath, std::fstream::in);
        if(txfile.fail()) {
            std::cerr << "Failed to open network statistics file: " << strerror(errno) << std::endl;
            throw std::runtime_error("");
        }

        std::ifstream rxfile;
        txfile.open(rxpath, std::fstream::in);
        if(rxfile.fail()) {
            std::cerr << "Failed to open network statistics file: " << strerror(errno) << std::endl;
            throw std::runtime_error("");
        }
        
        /*
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

        // fill tx statistics vector
        long tx_bytes;
        txfile >> tx_bytes;
        long rx_bytes;
        rxfile >> rx_bytes;
        long net_bytes = tx_bytes + rx_bytes;
        TxDataPoint txdatapoint(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()),
                net_bytes-last_net_bytes);
        last_net_bytes = net_bytes;
        tx_series.push_back(txdatapoint);
        net_count++;

        // sleep for a millisecond, may not be necessary
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        scanfile.clear();
    }

    // write last second into json
    /*
    auto now = std::chrono::high_resolution_clock::now();
    auto current_time = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
    auto begin_time = current_time - std::chrono::seconds(1);
    //auto index = std::lower_bound(tx_series.begin(), tx_series.end(), [&begin_time](const auto& s)
    //        {return std::get<0>(s) <= begin_time; });
    auto json = nlohmann::json::array();
    //json.push_back("asdf");
    for (auto i = tx_series.size()-500; i < tx_series.size(); i++) {
        json.push_back(std::get<1>(tx_series.at(i)));
    }
    auto dump = json.dump();
    auto ubjson = nlohmann::json::to_ubjson(json);
    //std::cout << ubjson.size() << std::endl;
    //auto back = nlohmann::json::from_ubjson(ubjson);
    //auto backdump = back.dump();
    //neighbor_manager->send_tx(json);
    */



    std::cout << "\ncaught SIGINT, writing data to disc\n";

    std::lock_guard<std::mutex> guard(file_lock);

    // output scan data
    for (auto const& sample: received_series) {
        sample->output(outputscanfile);
    }
    outputscanfile.close();

    // output tx data
    for (auto const& datapoint: tx_series) {
        outputtxfile << std::chrono::duration_cast<std::chrono::milliseconds>(std::get<0>(datapoint).time_since_epoch()).count()
            << ";" << std::get<1>(datapoint) << ";\n";
    }
    std::cout << "finished writing, exiting\n";
}

// read a sample from proc, convert it to an object and store it in a vector
void Collector::readSample(std::ifstream &scanfile, decltype(received_series) &received_series) {

    scanfile.peek(); // check for EOF
    if(scanfile.eof()) {
        throw std::runtime_error("EOF reached");
    }

    auto sample = new fft_sample_ath10k;
    scanfile.read((char*)&sample->tlv, sizeof(fft_sample_tlv)); //read TLV header
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
