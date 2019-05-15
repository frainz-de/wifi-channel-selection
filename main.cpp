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
#include <Eigen/Dense>

fft_sample_ath10k* readSample(std::ifstream &scanfile) {

    scanfile.peek(); // check for EOF
    if(scanfile.eof()) {
        throw std::runtime_error("EOF reached");
    }

    auto sample = new fft_sample_ath10k;
    scanfile.read((char*)&sample->tlv, sizeof(fft_sample_tlv)); //read TLV header
    std::cout << "type: " << unsigned(sample->tlv.type) << std::endl;
    std::cout << "length: " << be16toh(sample->tlv.length) << std::endl;
    if(sample->tlv.type != ATH_FFT_SAMPLE_ATH10K) {
        throw std::runtime_error("Wrong sample type, only ath10k samples are supportet atm\n");
    }

    // read rest of header
    scanfile.read((char*)sample + sizeof(fft_sample_tlv), sizeof(fft_sample_ath10k) - sizeof(fft_sample_tlv));
    // create buffer
    size_t datalength = be16toh(sample->tlv.length) - sizeof(fft_sample_ath10k) + sizeof(fft_sample_tlv);
    auto data = new char[datalength];
    scanfile.read(data, datalength );

    scanfile.peek(); //set EOF bit if no data available
    return sample;
}

int main(int argc, char* argv[]) {
    std::string path = "/sys/kernel/debug/ieee80211/phy1/ath10k/spectral_scan0";

    // hacky argument handling
    if(argc > 1) {
        path = argv[1];
    }

    // try to open (virtual) file in binary mode
    std::ifstream scanfile;
    scanfile.open(path ,std::fstream::in | std::fstream::binary);
    if(scanfile.fail()) {
        std::cerr << "Failed to read file: " << strerror(errno) << std::endl;
        return 1;
    }

    //remove characters until valid header is found
    if(scanfile.peek() != ATH_FFT_SAMPLE_ATH10K) {
        std::cerr << "Invalid header, discarding until valid\n";
    }
    while(scanfile.peek() != ATH_FFT_SAMPLE_ATH10K) {
        scanfile.ignore(1);
    }


    while (true) {
        auto now =  std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        //auto localtime = std::localtime(&in_time_t);
        //std::cout << std::put_time(localtime, "%Y-%m-%d %X");
        std::cout << std::ctime(&in_time_t) << std::endl;

        scanfile.peek();
        while(!scanfile.eof()) {
            readSample(scanfile);
        }
        sleep(1);
        std::cout << "waiting\n";
        scanfile.clear();
    }


    // scanfile.close(); // the destructor does this for us
    
    return 0;
}
