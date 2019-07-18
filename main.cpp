#include "sample.h"
#include "collector.h"
#include "neighbor_manager.h"
#include "helpers.h"
#include <iostream>
#include <unistd.h>
#include <thread>
#include <signal.h>
#include <tclap/CmdLine.h>
//#include <Eigen/Dense>

using DataPoint = std::tuple<int, double>;
using TxDataPoint = std::tuple<std::chrono::milliseconds, long>;


// global variable to exit main loop gracefully
volatile bool running = true;
int abortpipe[2];

// catch SIGINT to terminate gracefully
void signalHandler(int sig) {
    if(sig == SIGINT) {
        running = false;
        signal(SIGINT, SIG_DFL);
        write(abortpipe[1], "a", 1);
    }
}

int main(int argc, char* argv[]) {
    std::string interface = "wlp5s0";
    std::string phy;// = "phy1";
    std::string txpath;
    std::string scanpath;
    int freq = 0;

    // proper command line parsing
    try {
        TCLAP::CmdLine cmd("spectrometer and network correlation program");
        TCLAP::ValueArg<std::string> specinterface_arg("s", "specinterface",
                "Interface to use as spectrometer", false, "wlp1s0", "interface name", cmd);
        TCLAP::ValueArg<std::string> netinterface_arg("n", "netinterface",
                "Interface to use for networking", false, "wlp5s0", "interface name", cmd);
        TCLAP::ValueArg<int> freq_arg("f", "frequency",
                "Frequency to scan at", false, 0, "freq", cmd);

        cmd.parse(argc, argv);

        interface = specinterface_arg.getValue();

        freq = freq_arg.getValue();

    } catch (TCLAP::ArgException &e) {
        std::cerr << "error: " << e.error() << " for arg " << e.argId() << std::endl;
    }

    if(freq) {
        std::string mode = freq < 5000 ? "HT20" : "80MHz";
        std::system(("/usr/bin/iw dev " + interface + " set freq " + std::to_string(freq) + " " + mode).c_str());
    }

    signal(SIGINT, signalHandler);

    pipe(abortpipe);

    //std::thread neighbor_thread(manage_neighbors, interface);
    NeighborManager neighbor_manager(interface);
    std::thread neighbor_thread = neighbor_manager.start_thread(&running, abortpipe[0]);

    std::thread scan_thread([&] {neighbor_manager.scanandsend();});
    
    Collector collector(interface);
    std::thread collector_thread = collector.start_thread(&running);
    
    neighbor_thread.join();
    collector_thread.join();
    scan_thread.join();

    return 0;
}
