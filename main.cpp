extern "C" {
    #include "spectral_common.h"
}
#include <iostream>
#include <fstream>
#include <cstring>

int main(int, char*[]) {
    std::ifstream scanfile;
    scanfile.open("/sys/kernel/debug/ieee80211/phy1/ath10k/spectral_scan0",std::fstream::in | std::fstream::binary);
    if(scanfile.fail()) {
        std::cerr << "Failed to read file: " << strerror(errno) << std::endl;
        return 1;
    }


    auto sample = new fft_sample_ath10k;
    auto header = &(sample->tlv);
    scanfile.read((char*)header, sizeof(fft_sample_tlv)); //read in header
    std::cout << "type: " << unsigned(header->type) << std::endl;
    std::cout << "length: " << be16toh(header->length) << std::endl;
    if(sample->tlv.type != 3) {
        std::cerr << "Wrong sample type, only ath10k samples are supportet atm\n";
        return 2;
    }

    scanfile.read((char*)sample + sizeof(*header), sizeof(*sample) - sizeof(*header));

    scanfile.close();

}
