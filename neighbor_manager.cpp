#include "neighbor_manager.h"
#include "helpers.h"
#include "collector.h"
#include "wpa_ctrl.h"
#include "channel_strategy.h"

#include <nlohmann/json.hpp>
#include <iostream>
#include <set>
#include <sstream>
#include <algorithm>
#include <vector>
//sockets:
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/poll.h>
#include <sys/timerfd.h>
#include <unistd.h>

NeighborManager::NeighborManager(const std::string& specinterface, const std::string& netinterface)
    : specinterface(specinterface), netinterface(netinterface) {
    //TODO get this from a config file
    std::string prefix("fd00::1"); // prefix to filter for addresses
    own_address = exec("ip a | grep -o '" + prefix + ".*/' | tr -d '/' | tr -d '\n'");

    channel_strategy = new RandomChannelStrategy(specinterface, netinterface);

    //TODO remove temp stuff
    std::string exec_string("hostapd_cli -i " + netinterface + " status");
    std::string status = exec(exec_string);
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
    neighbor_string = exec("for i in $(iw dev " + specinterface + " scan -u | grep '42:42:42' |  awk '{ s = \"\"; for (i = 6; i <= NF; i++) s = s $i; print s }'); do echo $i | xxd -p -r; printf '\n'; done | sort");

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
    //std::cerr << "\nsending message: " << msg << std::endl;

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
       throw std::runtime_error("sending failed: " + std::string(strerror(errno)));
   }
   send_lock.unlock();

}

void NeighborManager::send_neighbors() {
    nlohmann::json neighbor_msg;
    neighbor_msg["self"]["address"] = own_address;
    neighbor_msg["self"]["channel"] = channel_strategy->get_netchannel();
    nlohmann::json neighbor_json(neighbors);
    neighbor_msg["neighbors"] = neighbor_json;

    std::string neighbor_msg_dump = neighbor_msg.dump();


    for(auto i = neighbors.begin(); i != neighbors.end(); i++) {
       send_msg(std::string(*i), neighbor_msg_dump);
    }
}

void NeighborManager::send_tx() {
    nlohmann::json msg;
    msg["self"]["address"] = own_address;
    msg["self"]["channel"] = channel_strategy->get_netchannel();
    auto txmsg = collector->get_tx(1500);
    msg["txmsg"] = txmsg;
    auto dump = msg.dump();
    // TODO use neighborsneighbors
    for(auto i = neighbors_neighbors.begin(); i != neighbors_neighbors.end(); i++) {
       send_msg(std::string(*i), dump);
    }
}


void NeighborManager::switch_channel(int channel) {
    std::string res = exec("hostapd_cli -i " + netinterface + " chan_switch 3 " + std::to_string(channel));
}


void NeighborManager::receive_message(int sockfd) {
    std::vector<char> buffer(65535);
    sockaddr_in6 src_addr;
    socklen_t addrlen = sizeof(src_addr);

    auto received_bytes = recvfrom(sockfd, (char *)buffer.data(), buffer.size(),
            MSG_WAITALL | MSG_TRUNC, (sockaddr*) &src_addr, &addrlen);
    if (received_bytes <= 0) {
        //auto err = std::strerror(errno);
        throw std::runtime_error(std::string("receiving failed: ") + std::strerror(errno));
    }
    if (received_bytes > signed(buffer.size())) {
        throw std::runtime_error("receive buffer too small");
    }

    // try to get receiver address, gives back garbage
    char src_addr_c[INET6_ADDRSTRLEN] = {};
    inet_ntop(src_addr.sin6_family, &src_addr.sin6_addr, src_addr_c, 50);
    std::string src_addr_s(src_addr_c);


    std::string msg(buffer.begin(), std::next(buffer.begin(), received_bytes));

    std::cerr << "\nreceived msg from " << src_addr_s << ": " << msg << std::endl;

    //TODO: catch parse errors
    nlohmann::json msg_json = nlohmann::json::parse(msg);


    if(msg_json.find("neighbors") != msg_json.end()) {
        auto received_neighbors = msg_json["neighbors"];

        std::stringstream output;
        output << "\nreceived neighbors: ";
        for(nlohmann::json::iterator i = received_neighbors.begin(); i!=received_neighbors.end(); i++) {
            output << *i << ", ";
            if (*i != own_address) {
                neighbors_neighbors.insert(i->get<std::string>());
            }
        }
        output << std::endl;
        std::cout << output.str();
    }

    if(msg_json.find("self") != msg_json.end()) {
        channels[msg_json["self"]["address"]] = msg_json["self"]["channel"];
        std::cout << "\nnoting channel " << msg_json["self"]["channel"]
            << " to " << msg_json["self"]["address"] << std::endl;
    }

    if(msg_json.find("txmsg") != msg_json.end()) {
        int channel = msg_json.at("self").at("channel");
        if(channel == channel_strategy->get_netchannel()) {
        //TODO put back in, only this is only for debugging
        //{
            auto txdata = msg_json["txmsg"]["txdata"];
            std::vector<long> txvector;
            txdata.get_to(txvector);
            auto timestamp = msg_json["txmsg"]["timestamp"];
            assert (txdata.size() == txvector.size());
            auto correlation = collector->correlate(txdata, timestamp);
            correlations[msg_json.at("self").at("address")] = correlation;
            channel_strategy->save_correlation(msg_json.at("self").at("address"), correlation,
                    std::chrono::time_point<Clock>(std::chrono::milliseconds(timestamp)));
            std::cout << ("\n correlation: " + std::to_string(correlation) + "\n");
        }
    }

}


void NeighborManager::run(volatile bool* running, int abortpipe) {

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
    //char buffer[1500] = {};

    struct sockaddr_in6 neighbor_addr = {};
    neighbor_addr.sin6_family = AF_INET6;
    neighbor_addr.sin6_port = htons(8901);
    neighbor_addr.sin6_addr = in6addr_any;
    bind(sockfd, (const struct sockaddr *) &neighbor_addr, sizeof(neighbor_addr));

    int timerfd = timerfd_create(CLOCK_REALTIME, 0);
    itimerspec spec = {{.tv_sec=1}, {.tv_sec=1}};
    assert(timerfd_settime(timerfd, 0, &spec, 0) >= 0);

    struct pollfd pfds[3];
    pfds[0].fd = abortpipe;
    pfds[0].events = POLLIN;
    pfds[1].fd = sockfd;
    pfds[1].events = POLLIN;
    pfds[2].fd = timerfd;
    pfds[2].events = POLLIN;

    //itimerspec time = {};
    //timerfd_gettime(timerfd, &time);

    while (*running) {
        //std::fill(buffer.begin(), buffer.end(), 0);
        poll(pfds, 3, -1);
        if(pfds[1].revents & POLLIN) {
            // use fd in pfds, because sockfd is miraculously set to zero at some point
            receive_message(pfds[1].fd);
        }
        if(pfds[2].revents & POLLIN) {
            int timersElapsed = 0;
            read(pfds[2].fd, &timersElapsed, 8);

            channel_strategy->do_something();
            send_tx();

            collector->truncate(std::chrono::seconds(20));
            //TODO: write to files while or before truncating
        }

    }

}
