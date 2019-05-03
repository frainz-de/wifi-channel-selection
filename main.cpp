extern "C" {
    #include "spectral_common.h"
}
#include <iostream>
#include <fstream>

int main(int, char*[]) {
    //std::ifstream scanfile("/sys/kernel/debug/ieee80211/phy1/ath10k");
    std::ifstream scanfile("/sys/kernel/debug/ieee80211/phy1/ani_enable");

    std::string line;
    //std::getline(scanfile, line);
    scanfile.get(line);
    std::cout << line << std::endl;

}
