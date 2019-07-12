#include "neighbor_manager.h"
#include "helpers.h"

#include <nlohmann/json.hpp>
#include <iostream>
#include <set>
//sockets:
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

NeighborManager::NeighborManager(const std::string& interface) : interface(interface) {
    //TODO get this from a config file
    std::string prefix("fd00::1"); // prefix to filter for addresses
    own_address = exec("ip a | grep -o '" + prefix + ".*/' | tr -d '/' | tr -d '\n'");
}


std::thread NeighborManager::start_thread(volatile bool* running) {
    std::thread thread(&NeighborManager::run, this, running);
    return thread;
}

void NeighborManager::scan() {
    std::cout << "\n starting scan\n";
    std::string neighbor_string;
    neighbor_string = exec("for i in $(iw dev " + interface + " scan -u | grep '42:42:42' |  awk '{ s = \"\"; for (i = 6; i <= NF; i++) s = s $i; print s }'); do echo $i | xxd -p -r; printf '\n'; done | sort");
    std::cout << std::endl << neighbor_string << std::endl;

    std::cout << "\n scan finished\n" << std::flush;

    // parse string with neighbor addresses into list
    std::set<std::string> neighbor_list;
    {
        char* pch;
        char* neighbors_cstr = new char[neighbor_string.length() + 1];
        strcpy(neighbors_cstr, neighbor_string.c_str());
        pch = strtok(neighbors_cstr, "\n");
        while(pch) {
            std::string neighbor(pch);
            if (neighbor != own_address) {
                neighbor_list.insert(neighbor);
            }
            pch = strtok(NULL, "\n");
        }

        delete[] neighbors_cstr;
    }
    neighbors = neighbor_list;

}

void NeighborManager::run(volatile bool* running) {
    scan();
    auto neighbor_list = neighbors;

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

       sendto(sockfd, neighbor_msg_dump.c_str(), neighbor_msg_dump.length(), 0, (struct sockaddr*)&neighbor_addr, sizeof(neighbor_addr));
       std::cout << *i << std::endl;

    }

    // logic to receive neighbors of neighbors --> start at the beginning of thread
    // maybe use std::future for scanning
    int sockfd = socket(AF_INET6, SOCK_DGRAM, 0);
    char buffer[1500] = {};

    struct sockaddr_in6 neighbor_addr = {};
    neighbor_addr.sin6_family = AF_INET6;
    neighbor_addr.sin6_port = htons(8901);
    neighbor_addr.sin6_addr = in6addr_any;
    bind(sockfd, (const struct sockaddr *) &neighbor_addr, sizeof(neighbor_addr));

    recvfrom(sockfd, (char *)buffer, sizeof(buffer), MSG_WAITALL, 0, 0);
    std::string msg(buffer);
    std::cout << std::endl << msg;

    nlohmann::json received_neighbors_json = nlohmann::json::parse(msg);
    auto received_neighbors = received_neighbors_json["neighbors"];

    for(auto i = received_neighbors.begin(); i!=received_neighbors.end(); i++) {
        std::cout << *i << std::endl;
    }


}
