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


// global variable to exit main loop gracefully
volatile bool running = true;
int abortpipe[2];
bool verbose;

// catch SIGINT to terminate gracefully
void signalHandler(int sig) {
    if(sig == SIGINT) {
        running = false;
        signal(SIGINT, SIG_DFL);
        write(abortpipe[1], "a", 1);
    }
}

int main(int argc, char* argv[]) {
    std::string specinterface = "wlp5s0";
    std::string netinterface;
    std::string phy;// = "phy1";
    std::string txpath;
    std::string scanpath;
    int freq = 0;
    //StrategyType strategytype;
    std::string strategytype;
//    bool verbose;

    // proper command line parsing
    try {
        TCLAP::CmdLine cmd("spectrometer and network correlation program");
        TCLAP::ValueArg<std::string> specinterface_arg("s", "specinterface",
                "Interface to use as spectrometer", false, "wlp1s0", "interface name", cmd);
        TCLAP::ValueArg<std::string> netinterface_arg("n", "netinterface",
                "Interface to use for networking", false, "wlp5s0", "interface name", cmd);
        TCLAP::ValueArg<int> freq_arg("f", "frequency",
                "Frequency to scan at", false, 0, "freq", cmd);
        TCLAP::ValueArg<std::string> strategy_arg("c", "channelstrategy",
                "Channel selection strategy to use", false, "correlation", "strategy type", cmd);
        TCLAP::SwitchArg verbose_arg("v", "verbose",
                "Produces more output to stdout", cmd);

        cmd.parse(argc, argv);

        specinterface = specinterface_arg.getValue();
        netinterface = netinterface_arg.getValue();
        freq = freq_arg.getValue();
        strategytype = strategy_arg.getValue();
        verbose = verbose_arg.getValue();

    } catch (TCLAP::ArgException &e) {
        std::cerr << "error: " << e.error() << " for arg " << e.argId() << std::endl;
    }

    if(freq) {
        std::string mode = freq < 5000 ? "HT20" : "80MHz";
        std::system(("/usr/bin/iw dev " + specinterface + " set freq " + std::to_string(freq) + " " + mode).c_str());
    }

    signal(SIGINT, signalHandler);

    // create pipe to abort select calls
    pipe(abortpipe);

    Collector collector(specinterface, netinterface);
    NeighborManager neighbor_manager(specinterface, netinterface, strategytype);

    collector.set_neighbor_manager(&neighbor_manager);
    neighbor_manager.set_collector(&collector);

    // create threads
    std::thread neighbor_thread = neighbor_manager.start_thread(&running, abortpipe[0]);
    pthread_setname_np(neighbor_thread.native_handle(), "neighbor_thread");
    std::thread scan_thread([&neighbor_manager] {neighbor_manager.scanandsend();});
    pthread_setname_np(scan_thread.native_handle(), "scan_thread");
    std::thread collector_thread = collector.start_thread(&running);
    pthread_setname_np(collector_thread.native_handle(), "collector_thread");

    // wait for all threads
    neighbor_thread.join();
    collector_thread.join();
    scan_thread.join();

    return 0;
}
