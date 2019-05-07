extern "C" {
    #include "spectral_common.h"
}
#include <iostream>
#include <fstream>
#include <cstring>

int main(int, char*[]) {
    //std::ifstream scanfile("/sys/kernel/debug/ieee80211/phy1/ath10k");
    std::ifstream scanfile;
    scanfile.open("/sys/kernel/debug/ieee80211/phy1/ath10k/spectral_scan0",std::fstream::in | std::fstream::binary);
    if(scanfile.fail()) {
        std::cerr << "Failed to read file: " << strerror(errno) << std::endl;
        return 1;
    }


    auto header = new fft_sample_tlv;
    //auto header = static_cast<fft_sample_tlv*>(headerbuffer);
    scanfile.read((char*)header, sizeof(fft_sample_tlv));
    uint8_t type = header->type;
    std::cout << "type: " << unsigned(type) << std::endl;
    std::cout << "length: " << be16toh(header->length) << std::endl;

    std::cout << "size of fft_sample_ath10k: " << sizeof(fft_sample_ath10k) << std::endl;

    scanfile.close();

}
