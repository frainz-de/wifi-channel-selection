extern "C" {
    #include "spectral_common.h"
}
#include <iostream>
#include <fstream>
#include <cstring>

int main(int, char*[]) {
    //std::ifstream scanfile("/sys/kernel/debug/ieee80211/phy1/ath10k");
    std::ifstream scanfile;
    scanfile.open("/sys/kernel/debug/ieee80211/phy1/ath10k/spectral_scan0");
    if(scanfile.fail()) {
        std::cerr << "Failed to read file\n";
        std::cerr << "Error: " << strerror(errno) << std::endl;
        return 1;
    }

    std::string line;
    //std::getline(scanfile, line);
    //scanfile.get(line);
    scanfile >> line;
    std::cout << line << std::endl;

    scanfile.close();

}
