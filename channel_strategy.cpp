#include "channel_strategy.h"
#include "neighbor_manager.h"
#include "helpers.h"

#include <chrono>
#include <thread>
#include <random>
#include <iostream>
#include <cassert>
#include <cstdlib>


ChannelStrategy::ChannelStrategy(NeighborManager* neighbor_manager, const std::string& specinterface, const std::string& netinterface):
    neighbor_manager(neighbor_manager), specinterface(specinterface), netinterface(netinterface) {

    specchannel = stoi(exec("iw dev " + specinterface + " info | grep channel | awk '{print $3}' | tr -d '('"));
    netchannel = stoi(exec("iw dev " + netinterface + " info | grep channel | awk '{print $3}' | tr -d '('"));
    };

void ChannelStrategy::switch_channel(int freq) {
    std::string res = exec("hostapd_cli -i " + netinterface + " chan_switch 1 " + std::to_string(freq));
    if (res != "OK\n") {
        std::cerr << "\n\033[31mfailed to set channel to " + std::to_string(freq) + ": " + res + "\033[0m";
    } else {
        std::cout << "\n\033[32msuccessfully set channel to " + std::to_string(freq) + "\033[0m\n";
        netchannel = stoi(exec("iw dev " + netinterface + " info | grep channel | awk '{print $3}' | tr -d '('"));
        assert(netchannel == freq);
    }
}

void ChannelStrategy::set_spec_channel(int freq) {
    //std::string res = exec("iw dev wlp1s0 set freq " + std::to_string(freq));
    auto res = std::system(("iw dev wlp1s0 set freq " + std::to_string(freq)).c_str());
    switch (res) {
    case 0:
        std::cout << "\n\033[32msuccessfully set scan channel to " + std::to_string(freq) + "\033[0m\n";
        break;
    case 240:
        std::cerr << "\n\033[31mfailed to set scan channel to " + std::to_string(freq) + ": device busy \033[0m";
        break;
    case 234:
        std::cerr << "\n\033[31mfailed to set scan channel to " + std::to_string(freq) + ": invalid argument \033[0m";
        break;
    }
}

void ChannelStrategy::save_correlation(std::string address, double correlation,
        std::chrono::time_point<Clock> timestamp) {

    //correlations[address] = {correlation, timestamp};
    correlations.insert({address, {correlation, timestamp}});
    std::cout << "\nnoting correlation " + std::to_string(correlation) + " for neighbor " + address + "\n";
}

void ChannelStrategy::record_channel(std::string address, int freq) {
    channels[address] = freq;
}

int ChannelStrategy::get_specchannel() {return specchannel;}
int ChannelStrategy::get_netchannel() {return netchannel;}


void CorrelationChannelStrategy::do_something() {
   // get neighbor with oldest / no correlation and change spec channel accordingly
    std::pair<std::string, std::tuple<double, std::chrono::time_point<Clock>>> oldest
        = {"", {0, std::chrono::time_point<Clock>::max()}};

    for (auto i = correlations.begin(); i != correlations.end(); i++) {
        if (std::get<1>(i->second) <= std::get<1>(oldest.second)) {
            oldest = *i;
        }
    }

    // change scanning channel to bring the oldest measurement up to date
    if (oldest.first != "") {
        //int channel = neighbor_manager->get_freq_from_neighbor(oldest.first);
        int channel = channels[oldest.first];
        set_spec_channel(channel);
    }
}

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
