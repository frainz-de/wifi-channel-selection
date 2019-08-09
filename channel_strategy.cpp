#include "channel_strategy.h"
#include "helpers.h"

#include <chrono>
#include <thread>
#include <random>
#include <iostream>
#include <cassert>


ChannelStrategy::ChannelStrategy(const std::string& specinterface, const std::string& netinterface):
    specinterface(specinterface), netinterface(netinterface) {

    specchannel = stoi(exec("iw dev " + specinterface + " info | grep channel | awk '{print $3}' | tr -d '('"));
    netchannel = stoi(exec("iw dev " + netinterface + " info | grep channel | awk '{print $3}' | tr -d '('"));
    };

void ChannelStrategy::switch_channel(int channel) {
    std::string res = exec("hostapd_cli -i " + netinterface + " chan_switch 1 " + std::to_string(channel));
    if (res != "OK\n") {
        std::cerr << "\n\033[31mfailed to set channel to " + std::to_string(channel) + ": " + res + "\033[0m";
    } else {
        std::cout << "\n\033[32msuccessfully set channel to " + std::to_string(channel) + "\033[0m\n";
        netchannel = stoi(exec("iw dev " + netinterface + " info | grep channel | awk '{print $3}' | tr -d '('"));
        assert(netchannel == channel);

    }

}
int ChannelStrategy::get_specchannel() {return specchannel;}
int ChannelStrategy::get_netchannel() {return netchannel;}

void RandomChannelStrategy::do_something() {
    std::chrono::time_point now = std::chrono::system_clock::now();
    if (now - last_checked < std::chrono::seconds(5)) {
        return;
    }

    last_checked = now;

    std::random_device random_device;
    std::mt19937 engine{random_device()};
    std::uniform_int_distribution<int> dist(0, possible_channels.size() - 1);

    int channel = possible_channels[dist(engine)];
    switch_channel(channel);

}
