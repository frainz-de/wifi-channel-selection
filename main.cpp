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
#include <dirent.h>
#include <list>
#include <set>
#include <nlohmann/json.hpp>
//sockets:
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
//#include <Eigen/Dense>
#include "sample.h"
#include "collector.h"
#include "neighbor_manager.h"
#include "helpers.h"

using DataPoint = std::tuple<int, double>;
using TxDataPoint = std::tuple<std::chrono::milliseconds, long>;


// global variable to exit main loop gracefully
volatile bool running = true;

// catch SIGINT to terminate gracefully
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

    if(channel) {
        std::string mode = channel < 5000 ? "HT20" : "80MHz";
        std::system(("/usr/bin/iw dev " + interface + " set freq " + std::to_string(channel) + " " + mode).c_str());
    }

    signal(SIGINT, signalHandler);

    //std::thread neighbor_thread(manage_neighbors, interface);
    NeighborManager neighbor_manager(interface);
    std::thread neighbor_thread = neighbor_manager.start_thread(&running);

    std::thread scan_thread([&] {neighbor_manager.scanandsend();});
    
    Collector collector(interface);
    std::thread collector_thread = collector.start_thread(&running);
    
    neighbor_thread.join();
    collector_thread.join();
    scan_thread.join();

    return 0;
}
