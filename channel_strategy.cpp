#include "channel_strategy.h"
#include "neighbor_manager.h"
#include "helpers.h"

#include <chrono>
#include <thread>
#include <random>
#include <iostream>
#include <cassert>
#include <cstdlib>
#include <cmath>


ChannelStrategy::ChannelStrategy(NeighborManager* neighbor_manager, const std::string& specinterface, const std::string& netinterface):
    neighbor_manager(neighbor_manager), specinterface(specinterface), netinterface(netinterface) {

    std::string specchannel_string  = exec("iw dev " + specinterface + " info | grep channel | awk '{print $3}' | tr -d '('");
    if (!specchannel_string.empty()) {
        specchannel = stoi(specchannel_string);
    } else {
        std::cerr << "\n\e[31mcould not deduce spectral channel\e[0m\n";
    }

    std::string netchannel_string  = exec("iw dev " + netinterface + " info | grep channel | awk '{print $3}' | tr -d '('");
    if (!netchannel_string.empty()) {
        netchannel = std::stoi(netchannel_string);
    } else {
        std::cerr << "\n\e[31mcould not deduce network channel\e[0m\n";
    }
}

void ChannelStrategy::switch_channel(int freq) {
    int netchannel_local = stoi(exec("iw dev " + netinterface + " info | grep channel | awk '{print $3}' | tr -d '('"));
    assert(netchannel == netchannel_local);

    std::string res = exec("hostapd_cli -i " + netinterface + " chan_switch 1 " + std::to_string(freq));
    rtrim(res);
    if (res == "OK") {
        std::cout << "\n\033[42msuccessfully set channel to " + std::to_string(freq) + "\033[0m\n";
        netchannel = stoi(exec("iw dev " + netinterface + " info | grep channel | awk '{print $3}' | tr -d '('"));
        assert(netchannel == freq);
    } else {
        std::cerr << "\n\033[41mfailed to set channel to " + std::to_string(freq) + ": " + res + "\033[0m\n";
    }
}

void ChannelStrategy::set_spec_channel(int freq) {
    //std::string res = exec("iw dev wlp1s0 set freq " + std::to_string(freq));
    auto res = WEXITSTATUS(std::system(("iw dev " + specinterface + " set freq " + std::to_string(freq)).c_str()));
    specchannel = stoi(exec("iw dev " + specinterface + " info | grep channel | awk '{print $3}' | tr -d '('"));
    switch (res) {
    case 0:
        std::cout << "\n\033[32msuccessfully set scan channel to " + std::to_string(freq) + "\033[0m\n";
        break;
    case 240:
        std::cerr << "\n\033[31mfailed to set scan channel to " + std::to_string(freq) + ": device busy \033[0m\n";
        break;
    case 234:
        std::cerr << "\n\033[31mfailed to set scan channel to " + std::to_string(freq) + ": invalid argument \033[0m\n";
        break;
    default:
        std::cerr << "\n\033[31mfailed to set scan channel to " + std::to_string(freq)
            + ", error code: " + std::to_string(res) + "\033[0m\n";
    }
}

void ChannelStrategy::save_correlation(std::string address, double correlation,
        std::chrono::time_point<Clock> timestamp) {

    //correlations[address] = {correlation, timestamp};
    correlations.insert({address, {correlation, timestamp}});
    std::cout << "\nnoting correlation \e[33m" + std::to_string(correlation)
        + "\e[36m for neighbor \e[36m" + address + "\e[0m\n";
}

void ChannelStrategy::record_channel(std::string address, int freq) {
    channels[address] = freq;
}

int ChannelStrategy::get_least_used_channel() {
    std::map<int, double> usage_map; //channel, usage
    //initialize
    for (auto i = possible_channels.begin(); i != possible_channels.end(); i++) {
        usage_map[*i] = 0;
    }

    for (auto i = correlations.begin(); i != correlations.end(); i++) {
        usage_map[channels[i->first]] += std::get<0>(i->second);
    }

    auto least_used = std::min_element(usage_map.begin(), usage_map.end(),
            [](const auto& l, const auto& r) -> bool {return std::abs(l.second) < std::abs(r.second);});

    return least_used->first;
}

int ChannelStrategy::get_specchannel() {return specchannel;}
int ChannelStrategy::get_netchannel() {return netchannel;}


void CorrelationChannelStrategy::do_something() {
    //get neighbor with oldest / no correlation and change spec channel accordingly

    std::pair<std::string, std::tuple<double, std::chrono::time_point<Clock>>> oldest
        = {"", {0, std::chrono::time_point<Clock>::max()}};

    for (auto i = channels.begin(); i != channels.end(); i++) {
        //if (std::get<1>(i->second) <= std::get<1>(oldest.second)) {

        if (std::get<1>(correlations[std::get<0>(*i)]) <= std::get<1>(std::get<1>(oldest))) {
            //oldest = correlations[std::get<0>(*i)];
            //oldest = *i;
            oldest.first = std::get<0>(*i);
            oldest.second = correlations[std::get<0>(*i)];
        }
    }

    // change scanning channel to bring the oldest measurement up to date
    if (oldest.first != "") {
        //int channel = neighbor_manager->get_freq_from_neighbor(oldest.first);
        int channel = channels[oldest.first];
        if (channel != specchannel) {
            set_spec_channel(channel);
        }
    }

    int least_used = get_least_used_channel();
    if (least_used != netchannel) {
        switch_channel(least_used);
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
