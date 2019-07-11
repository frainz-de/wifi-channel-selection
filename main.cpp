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
#include <nlohmann/json.hpp>
//sockets:
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
//#include <Eigen/Dense>
#include "sample.h"
#include "collector.h"

using DataPoint = std::tuple<int, double>;
using TxDataPoint = std::tuple<std::chrono::milliseconds, long>;

// global variable to exit main loop gracefully
volatile bool running = true;

static inline void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}


// execute system command and return stdout as string
std::string exec(const std::string cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

// catch SIGINT to terminate gracefully
void signalHandler(int sig) {
    if(sig == SIGINT) {
        running = false;
        signal(SIGINT, SIG_DFL);
    }
}

void manage_neighbors(const std::string& interface) {
    signal(SIGINT, signalHandler);
    std::cout << "\n starting scan\n";
    std::string neighbors;
    neighbors = exec("for i in $(iw dev " + interface + " scan -u | grep '42:42:42' |  awk '{ s = \"\"; for (i = 6; i <= NF; i++) s = s $i; print s }'); do echo $i | xxd -p -r; printf '\n'; done | sort");
    std::cout << std::endl << neighbors << std::endl;

    std::cout << "\n scan finished\n" << std::flush;

    std::list<std::string> neighbor_list;
    {
        char* pch;
        char* neighbors_cstr = new char[neighbors.length() + 1];
        strcpy(neighbors_cstr, neighbors.c_str());
        pch = strtok(neighbors_cstr, "\n");
        while(pch) {
            neighbor_list.push_back(std::string(pch));
            pch = strtok(NULL, "\n");
        }

        delete[] neighbors_cstr;
    }

    nlohmann::json neighbor_msg;
    nlohmann::json neighbor_json(neighbor_list);
    neighbor_msg["neighbors"] = neighbor_json;

    std::string neighbor_msg_dump = neighbor_msg.dump();

    // get neighbors regularly, not working yet
    /*
    std::chrono::time_point<std::chrono::system_clock> lastrun = 
        std::chrono::system_clock::now() - std::chrono::seconds(60);;
    while (running) {
        if ((lastrun - std::chrono::system_clock::now()) > std::chrono::seconds(60)) {
            neighbors = exec("for i in $(iw dev " + interface + " scan -u | grep '42:42:42' |  awk '{ s = \"\"; for (i = 6; i <= NF; i++) s = s $i; print s }'); do echo $i | xxd -p -r; printf '\n'; done | sort");
        std::cout << std::endl << neighbors << std::endl;
        lastrun = std::chrono::system_clock::now();
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));

    }
    */

    for(auto i = neighbor_list.begin(); i != neighbor_list.end(); i++) {
       int sockfd = socket(AF_INET6, SOCK_DGRAM, 0);
       struct sockaddr_in6 neighbor_addr = {};
       neighbor_addr.sin6_family = AF_INET6;
       neighbor_addr.sin6_port = htons (8901);
       //neighbor_addr.sin6_addr.s6_addr= u6_addr8(i->c_str());
       int e = inet_pton(AF_INET6, i->c_str(), &(neighbor_addr.sin6_addr));
       if (e <= 0) {
          throw std::runtime_error("address parsing failed");
       }

       //TODO: filter own address
       sendto(sockfd, neighbor_msg_dump.c_str(), neighbor_msg_dump.length(), 0, (struct sockaddr*)&neighbor_addr, sizeof(neighbor_addr));
       std::cout << *i << std::endl;

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

    std::thread neighbor_thread(manage_neighbors, interface);
    
    Collector collector(interface);
    std::thread collector_thread = collector.start_thread(&running);
    
    neighbor_thread.join();
    collector_thread.join();

    return 0;
}
