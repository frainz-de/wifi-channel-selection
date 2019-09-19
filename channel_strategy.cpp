#include "channel_strategy.h"
#include "neighbor_manager.h"
#include "helpers.h"
#include "collector.h"

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

    // initialize channel_power_map

    for (auto i = possible_channels.begin(); i != possible_channels.end(); i++) {
        channel_power_map[*i] = {0, std::chrono::time_point<Clock>::min()};
    }
}

void ChannelStrategy::set_net_channel(int freq) {
    int netchannel_local = stoi(exec("iw dev " + netinterface + " info | grep channel | awk '{print $3}' | tr -d '('"));
    assert(netchannel == netchannel_local);

    std::string res = exec("hostapd_cli -i " + netinterface + " chan_switch 1 " + std::to_string(freq));
    rtrim(res);
    if (res == "OK") {
        std::cout << "\n\033[42msuccessfully set channel to " + std::to_string(freq) + "\033[0m\n";
        netchannel = stoi(exec("iw dev " + netinterface + " info | grep channel | awk '{print $3}' | tr -d '('"));
        last_net_channel_switch = Clock::now();
        assert(netchannel == freq);
    } else {
        std::cerr << "\n\033[41mfailed to set channel to " + std::to_string(freq) + ": " + res + "\033[0m\n";
    }
}

void ChannelStrategy::set_spec_channel(int freq) {
    //std::string res = exec("iw dev wlp1s0 set freq " + std::to_string(freq));
    auto res = WEXITSTATUS(std::system(("iw dev " + specinterface + " set freq " + std::to_string(freq)).c_str()));
    switch (res) {
    case 0:
        std::cout << "\n\033[32msuccessfully set scan channel to " + std::to_string(freq) + "\033[0m\n";
        specchannel = stoi(exec("iw dev " + specinterface + " info | grep channel | awk '{print $3}' | tr -d '('"));
        last_spec_channel_switch = Clock::now();
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

    correlations[address] = {correlation, timestamp};
    //correlations.insert_or_assign({address, {correlation, timestamp}});
    std::cout << "\nnoting correlation \e[33m" + std::to_string(correlation)
        + "\e[36m for neighbor \e[36m" + address + "\e[0m\n";
}

void ChannelStrategy::save_power_sample() {
    try {
    auto rx_power = collector->get_rx_power(std::chrono::seconds(1));
    channel_power_map[std::get<0>(rx_power)] = {std::get<1>(rx_power), std::get<2>(rx_power)};
    } catch (std::runtime_error& e) {
        std::cerr << "\n" + std::string(e.what()) + "\n";
    }
}

void ChannelStrategy::record_channel(std::string address, int freq) {
    neighbor_channel_map[address] = freq;
}

void ChannelStrategy::set_collector(Collector* collector) {
    this->collector = collector;
}

int SimpleCorrelationChannelStrategy::get_least_used_channel() {
    std::map<int, double> usage_map; //channel, usage
    //initialize
    for (auto i = possible_channels.begin(); i != possible_channels.end(); i++) {
        usage_map[*i] = 0;
    }

    for (auto i = correlations.begin(); i != correlations.end(); i++) {
        usage_map[neighbor_channel_map[i->first]] += std::get<0>(i->second);
    }

    auto least_used = std::min_element(usage_map.begin(), usage_map.end(),
            [](const auto& l, const auto& r) -> bool {return std::abs(l.second) < std::abs(r.second);});

    return least_used->first;
}

int CorrelationChannelStrategy::pick_channel() {
    std::map<int, double> usage_map; //channel, usage
    //initialize
    for (auto i = possible_channels.begin(); i != possible_channels.end(); i++) {
        usage_map[*i] = 0;
    }

    for (auto i = correlations.begin(); i != correlations.end(); i++) {
        usage_map[neighbor_channel_map[i->first]] += std::get<0>(i->second);
    }
    for (auto i = channel_power_map.begin(); i != channel_power_map.end(); i++) {
        usage_map[i->first] += std::get<0>(i->second);
    }

    //find least used channel
    auto least_used = std::min_element(usage_map.begin(), usage_map.end(),
            [](const auto& l, const auto& r) -> bool {return std::abs(l.second) < std::abs(r.second);});
    return least_used->first;
}

int ChannelStrategy::get_oldest_neighbor_scanchannel() {
    //get neighbor with oldest / no correlation and change spec channel accordingly
    std::pair<std::string, std::tuple<double, std::chrono::time_point<Clock>>> oldest
        = {"", {0, std::chrono::time_point<Clock>::max()}};

    for (auto i = neighbor_channel_map.begin(); i != neighbor_channel_map.end(); i++) {
        if (std::get<1>(correlations[std::get<0>(*i)]) <= std::get<1>(std::get<1>(oldest))) {
            oldest.first = std::get<0>(*i);
            oldest.second = correlations[std::get<0>(*i)];
        }
    }
    if (oldest.first != "") {
        return neighbor_channel_map[oldest.first];
    } else {
        return 0;
    }
}

int ChannelStrategy::get_oldest_power_scanchannel() {
    std::pair<int, std::tuple<double, std::chrono::time_point<Clock>>> oldest
        = {0, {0, std::chrono::time_point<Clock>::max()}};

    for (auto i = channel_power_map.begin(); i != channel_power_map.end(); i++) {
        if (std::get<1>(std::get<1>(*i)) <= std::get<1>(std::get<1>(oldest))) {
            oldest = *i;
        }
    }

    auto size = channel_power_map.size();

    return oldest.first;
}

int ChannelStrategy::get_specchannel() {return specchannel;}
int ChannelStrategy::get_netchannel() {return netchannel;}


void CorrelationChannelStrategy::do_something() {
    //get neighbor with oldest / no correlation and change spec channel accordingly

    int oldest_scanchannel = get_oldest_power_scanchannel();
    if (oldest_scanchannel != specchannel && oldest_scanchannel != 0) {
        set_spec_channel(oldest_scanchannel);
    }

    // set networking channel to least used
    int new_channel = pick_channel();
    if (new_channel != netchannel) {
        set_net_channel(new_channel);
    }

    if (Clock::now() - last_spec_channel_switch > std::chrono::seconds(1)) {
        save_power_sample();
    }
}

void SimpleCorrelationChannelStrategy::do_something() {
    //get neighbor with oldest / no correlation and change spec channel accordingly

    int oldest_scanchannel = get_oldest_neighbor_scanchannel();
    if (oldest_scanchannel != specchannel && oldest_scanchannel != 0) {
        set_spec_channel(oldest_scanchannel);
    }

    // set networking channel to least used
    int least_used = get_least_used_channel();
    if (least_used != netchannel) {
        set_net_channel(least_used);
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
    set_net_channel(channel);

}
