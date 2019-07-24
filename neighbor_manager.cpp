#include "neighbor_manager.h"
#include "helpers.h"
#include "collector.h"

#include <nlohmann/json.hpp>
#include <iostream>
#include <set>
#include <sstream>
#include <algorithm>
//sockets:
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/poll.h>

NeighborManager::NeighborManager(const std::string& interface) : interface(interface) {
    //TODO get this from a config file
    std::string prefix("fd00::1"); // prefix to filter for addresses
    own_address = exec("ip a | grep -o '" + prefix + ".*/' | tr -d '/' | tr -d '\n'");
    own_channel = stoi(exec("iw dev wlp5s0 info | grep channel | awk '{print $3}' | tr -d '('"));
}


std::thread NeighborManager::start_thread(volatile bool* running, int abortpipe) {
    std::thread thread(&NeighborManager::run, this, running, abortpipe);
    return thread;
}

void NeighborManager::scanandsend() {
    scan();
    send_neighbors();
    send_tx();
}

void NeighborManager::set_collector(Collector* collector) {
    this->collector = collector;
}

void NeighborManager::scan() {
    // TODO: scan regularly

    std::cout << "\nstarting scan\n";
    std::string neighbor_string;
    neighbor_string = exec("for i in $(iw dev " + interface + " scan -u | grep '42:42:42' |  awk '{ s = \"\"; for (i = 6; i <= NF; i++) s = s $i; print s }'); do echo $i | xxd -p -r; printf '\n'; done | sort");

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
    neighbors_neighbors.insert(neighbors.begin(), neighbors.end());

    std::replace(neighbor_string.begin(), neighbor_string.end(), '\n', ',');
    std::cout << "\nscan finished, neighbors: " + neighbor_string + "\n" << std::flush;
}

void NeighborManager::send_msg(const std::string address, const std::string msg) {
   int sockfd = socket(AF_INET6, SOCK_DGRAM, 0);
   struct sockaddr_in6 addr_struct = {};
   addr_struct.sin6_family = AF_INET6;
   addr_struct.sin6_port = htons (8901);
   //addr_struct.sin6_addr.s6_addr= u6_addr8(i->c_str());
   int e = inet_pton(AF_INET6, address.c_str(), &(addr_struct.sin6_addr));
   if (e <= 0) {
      throw std::runtime_error("address parsing failed");
   }

   send_lock.lock();
   e = sendto(sockfd, msg.c_str(), msg.length(), 0, (struct sockaddr*)&addr_struct, sizeof(addr_struct));
   if (e == -1) {
       throw std::runtime_error("sending failed");
   }
   send_lock.unlock();

}

void NeighborManager::send_neighbors() {
    nlohmann::json neighbor_msg;
    //nlohmann::json self;
    neighbor_msg["self"]["address"] = own_address;
    neighbor_msg["self"]["channel"] = own_channel;
    nlohmann::json neighbor_json(neighbors);
    neighbor_msg["neighbors"] = neighbor_json;

    std::string neighbor_msg_dump = neighbor_msg.dump();


    for(auto i = neighbors.begin(); i != neighbors.end(); i++) {
       send_msg(std::string(*i), neighbor_msg_dump);
    }
}

void NeighborManager::send_tx() {
    nlohmann::json msg;
    msg["txdata"] = collector->get_tx(500);
    auto dump = msg.dump();
    // TODO use neighborsneighbors
    for(auto i = neighbors_neighbors.begin(); i != neighbors_neighbors.end(); i++) {
       send_msg(std::string(*i), dump);
    }
}

void NeighborManager::run(volatile bool* running, int abortpipe) {
    //scan();
    //send_neighbors();


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

    // logic to receive neighbors of neighbors --> start at the beginning of thread
    // maybe use std::future for scanning
    int sockfd = socket(AF_INET6, SOCK_DGRAM, 0);
    char buffer[1500] = {};

    struct sockaddr_in6 neighbor_addr = {};
    neighbor_addr.sin6_family = AF_INET6;
    neighbor_addr.sin6_port = htons(8901);
    neighbor_addr.sin6_addr = in6addr_any;
    bind(sockfd, (const struct sockaddr *) &neighbor_addr, sizeof(neighbor_addr));

    struct pollfd pfds[2];
    pfds[0].fd = abortpipe; 
    pfds[0].events = POLLIN;
    pfds[1].fd = sockfd;
    pfds[1].events = POLLIN;


    while (*running) {
        poll(pfds, 2, 0);
        if(pfds[1].revents != POLLIN) {
            continue;
        }

        recvfrom(sockfd, (char *)buffer, sizeof(buffer), MSG_WAITALL, 0, 0);
        std::string msg(buffer);

        std::cerr << "\nreceived msg: " << msg << std::endl;

        //TODO: catch parse errors
        nlohmann::json msg_json = nlohmann::json::parse(msg);


        if(msg_json.find("neighbors") != msg_json.end()) {
            auto received_neighbors = msg_json["neighbors"];

            std::stringstream output;
            output << "\nreceived neighbors: ";
            for(nlohmann::json::iterator i = received_neighbors.begin(); i!=received_neighbors.end(); i++) {
                output << *i << ", ";
                neighbors_neighbors.insert(i->get<std::string>());
            }
            output << std::endl;
            std::cout << output.str();
        }

        if(msg_json.find("self") != msg_json.end()) {
            channels[msg_json["self"]["address"]] = msg_json["self"]["channel"];
            std::cout << "\nnoting channel " << msg_json["self"]["channel"]
                << " to " << msg_json["self"]["address"] << std::endl;
        }
    }

}
