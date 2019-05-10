extern "C" {
    #include "spectral_common.h"
}
#include <iostream>
#include <fstream>
#include <cstring>
#include <stdio.h>
#include <unistd.h>

fft_sample_ath10k* readSample(std::ifstream &scanfile) {

    auto sample = new fft_sample_ath10k;
    scanfile.read((char*)&sample->tlv, sizeof(fft_sample_tlv)); //read in header
    std::cout << "type: " << unsigned(sample->tlv.type) << std::endl;
    std::cout << "length: " << be16toh(sample->tlv.length) << std::endl;
    if(sample->tlv.type != 3) {
        throw std::runtime_error("Wrong sample type, only ath10k samples are supportet atm\n");
    }

    scanfile.read((char*)sample + sizeof(fft_sample_tlv), sizeof(*sample) - sizeof(fft_sample_tlv));
    auto data = new char[be16toh(sample->tlv.length) - sizeof(fft_sample_ath10k) + sizeof(fft_sample_tlv)];
//    scanfile.read(data, be16toh(sample->tlv.length) - sizeof(fft_sample_ath10k) + sizeof(fft_sample_tlv));
    scanfile.read(data, 64);
    return sample;
}

int main(int argc, char* argv[]) {
    std::string path = "/sys/kernel/debug/ieee80211/phy1/ath10k/spectral_scan0";

    if(argc > 1) {
        path = argv[1];
    }


    std::ifstream scanfile;
    scanfile.open(path ,std::fstream::in | std::fstream::binary);
    if(scanfile.fail()) {
        std::cerr << "Failed to read file: " << strerror(errno) << std::endl;
        return 1;
    }
    readSample(scanfile);


    // scanfile.close(); // the destructor does this for us
    
    return 0;
}
