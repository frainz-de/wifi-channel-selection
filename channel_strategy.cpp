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
#include <iomanip>


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

    std::string stringseed = exec("hostname | md5sum | cut -c 1-15");
    long seed = std::stol(stringseed, 0, 16);
    generator = std::mt19937_64(seed+random_seed);
    double_dist = std::uniform_real_distribution<double>(0, 1);

}

void ChannelStrategy::set_net_channel(int freq) {
    int netchannel_local = stoi(exec("iw dev " + netinterface + " info | grep channel | awk '{print $3}' | tr -d '('"));
    if (netchannel != netchannel_local) {
        return;
    }

    std::string res = exec("hostapd_cli -i " + netinterface + " chan_switch 3 " + std::to_string(freq)
            + " center_freq1=" + std::to_string(freq) + " bandwidth=20 vht");
    rtrim(res);
    if (res == "OK") {
        std::cout << "\n\033[42msuccessfully set channel to " + std::to_string(freq) + "\033[0m\n";
        netchannel = stoi(exec("iw dev " + netinterface + " info | grep channel | awk '{print $3}' | tr -d '('"));
        last_net_channel_switch = Clock::now();
        //assert(netchannel == freq);
    } else {
        std::cerr << "\n\033[41mfailed to set channel to " + std::to_string(freq) + ": " + res + "\033[0m\n";
    }
}

void ChannelStrategy::set_spec_channel(int freq) {
    //std::string res = exec("iw dev wlp1s0 set freq " + std::to_string(freq));
    auto res = WEXITSTATUS(std::system(("iw dev " + specinterface
                    + " set freq " + std::to_string(freq) + " HT20").c_str()));
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

    if(correlations.find(address) != correlations.end()) {
        double new_average = correlation*0.4d + std::get<0>(correlations[address])*0.6d;
        correlations[address] = {new_average, timestamp};
    } else {
        correlations[address] = {correlation, timestamp};
    }
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
    std::map<int, double> channel_probabilities; //channel, usage
    // initialize
    for (auto i = possible_channels.begin(); i != possible_channels.end(); i++) {
        channel_probabilities[*i] = 1.0d / possible_channels.size();
    }

    // apply penalties for correlations
    // assume values from 0 to 1, maximum penalty: 60%
    // values usually go from 0 to 0.3
    for (auto i = correlations.begin(); i != correlations.end(); i++) {
        channel_probabilities[neighbor_channel_map[i->first]] *= 1.0d - std::get<0>(i->second)*0.6d/0.3d;
    }
    // apply penalties for received power
    // assume values from 0 to 20, maximum penalty: 20%
    for (auto i = channel_power_map.begin(); i != channel_power_map.end(); i++) {
        channel_probabilities[i->first] *= 1.0d - std::get<0>(i->second)*0.05d*0.2d;
    }
    // apply penalty for neighbors channel
    auto direct_neighbors = neighbor_manager->get_direct_neighbors();
    //for (auto i = neighbor_channel_map.begin(); i != neighbor_channel_map.end(); i++) {
    //    channel_probabilities[std::get<1>(*i)] *= (1-0.4d);
    for (auto i = direct_neighbors.begin(); i != direct_neighbors.end(); i++) {
        channel_probabilities[neighbor_channel_map[*i]] *= (1.0d-0.3d);
    }

    // apply gain for current channel to add inertia: 20%
    channel_probabilities[netchannel] *= 1.2d;

    double max = 0;
    int max_chan;
    for( auto i = channel_probabilities.begin(); i != channel_probabilities.end(); i++) {
        double prob = std::get<1>(*i);
        if(prob > max) {
            max = prob;
            max_chan = std::get<0>(*i);
        }
    }

    return max_chan;

    // normalize
    double sum = 0;
    for (auto i = channel_probabilities.begin(); i != channel_probabilities.end(); i++) {
        sum += i->second;
    }
    for (auto i = channel_probabilities.begin(); i != channel_probabilities.end(); i++) {
        i->second /= sum;
    }

    //double proper_sum = 0; //should be 1 now
    //for (auto& element : channel_probabilities) {
    //    proper_sum += element.second;
    //}


    // pick channel by probabilities in map

    double random_number = double_dist(generator);

    int channel = 0;
    for (auto i = channel_probabilities.begin(); i != channel_probabilities.end() && random_number > 0; i++) {
        random_number -= i->second;
        channel = i->first;
    }

    return channel;

    // preliminary hack: use channel with highest probability
    auto least_used = std::max_element(channel_probabilities.begin(), channel_probabilities.end(),
            [](const auto& l, const auto& r) -> bool {return std::abs(l.second) < std::abs(r.second);});

    return least_used->first;
}

int CorrelationChannelStrategy::pick_scanchannel() {
    auto powchan_and_time = get_oldest_power_scanchannel();
    if (std::get<1>(powchan_and_time) < Clock::now() - std::chrono::seconds(30)) {
        return std::get<0>(powchan_and_time);
    } else {
        return get_oldest_neighbor_scanchannel();
    }
}

int ChannelStrategy::get_oldest_neighbor_scanchannel() {
    // get neighbor with oldest / no correlation and change spec channel accordingly
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

std::tuple<int, std::chrono::time_point<Clock>> ChannelStrategy::get_oldest_power_scanchannel() {
    std::pair<int, std::tuple<double, std::chrono::time_point<Clock>>> oldest
        = {0, {0, std::chrono::time_point<Clock>::max()}};

    //TODO: include and priorize correlation channel info
    for (auto i = channel_power_map.begin(); i != channel_power_map.end(); i++) {
        if (std::get<1>(std::get<1>(*i)) <= std::get<1>(std::get<1>(oldest))) {
            oldest = *i;
        }
    }

    //auto size = channel_power_map.size();

    return {oldest.first, std::get<1>(oldest.second)};
}

int ChannelStrategy::get_specchannel() {return specchannel;}
int ChannelStrategy::get_netchannel() {return netchannel;}


void ChannelStrategy::print_correlations() {
    //nlohmann::json correlations_json = nlohmann::to_json<(correlations);
    nlohmann::json correlations_json;
    for( auto i = correlations.begin(); i != correlations.end(); i++) {
        correlations_json[std::get<0>(*i)] = std::get<0>(std::get<1>(*i));
    }
    std::ofstream file("/root/correlations.json");
    file << correlations_json << std::endl;
}

void ChannelStrategy::print_neighbor_channels() {
    nlohmann::json neighbor_channels_json;
    for(auto i = neighbor_channel_map.begin(); i != neighbor_channel_map.end(); i++) {
        neighbor_channels_json[std::get<0>(*i)] = std::get<1>(*i);
    }
    neighbor_channels_json[neighbor_manager->get_own_address()] = netchannel;
    std::ofstream file("/root/neighbor_channels.json");
    file << neighbor_channels_json << std::endl;
}

void ChannelStrategy::print_power_samples() {
    nlohmann::json power_samples_json;
    for(auto i = channel_power_map.begin(); i != channel_power_map.end(); i++) {
        std::string channel = std::to_string(std::get<0>(*i));
        power_samples_json[channel] = std::get<0>(std::get<1>(*i));
        std::ofstream file("/root/power_samples.json");
        file << power_samples_json << std::endl;
    }
}

bool ChannelStrategy::enough_correlations() {
    int counter = 0;
    std::set<std::string> partners = neighbor_manager->get_neighbors();
    for(auto i = partners.begin(); i != partners.end(); i++) {
        if(correlations.find(*i) != correlations.end()) {
            counter++;
        }
    }

    return (counter >= 0.65 * partners.size());
    //return complete;
}

void CorrelationChannelStrategy::do_something() {
    netchannel = stoi(exec("iw dev " + netinterface + " info | grep channel | awk '{print $3}' | tr -d '('"));

    //get neighbor with oldest / no correlation and change spec channel accordingly
    //TODO replace by prioritized scan channel choice
    //int new_scanchannel = std::get<0>(get_oldest_power_scanchannel());
    int new_scanchannel = pick_scanchannel();

    if (new_scanchannel != specchannel && new_scanchannel != 0) {
        set_spec_channel(new_scanchannel);
    }

    if (Clock::now() - last_spec_channel_switch > std::chrono::seconds(1)) {
        save_power_sample();
    }

    // set netchannel more rarely
    //std::chrono::time_point now = std::chrono::system_clock::now();
    //if (now - last_checked < std::chrono::seconds(5)) {
    //    return;
    //}
    //last_checked = now;

    if(Clock::now() < next_change) {
        return;
    }

    std::uniform_int_distribution<int> int_dist(30, 60);
    next_change = Clock::now() + std::chrono::seconds(int_dist(generator));


    // check if we have enough data
    //std::cout << std::put_time(init_time);
    //if(std::chrono::system_clock::now() - init_time < std::chrono::seconds(90)) {
        //std::cout << "\ntoo early to change channel\n";
    //}
    if (!enough_correlations()) {
        std::cout << "\n\e[41mnot enough correlations to change channel\033[0m\n";
        return;
    }
    // set networking channel to least used
    int new_channel = pick_channel();
    if (new_channel != netchannel) {
        set_net_channel(new_channel);
    }
}

void SimpleCorrelationChannelStrategy::do_something() {
    netchannel = stoi(exec("iw dev " + netinterface + " info | grep channel | awk '{print $3}' | tr -d '('"));

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
    netchannel = stoi(exec("iw dev " + netinterface + " info | grep channel | awk '{print $3}' | tr -d '('"));

    std::chrono::time_point now = std::chrono::system_clock::now();
    if (now - last_checked < std::chrono::seconds(5)) {
        return;
    }

    last_checked = now;

    std::uniform_int_distribution<int> int_dist(0, possible_channels.size() - 1);

    int channel = possible_channels[int_dist(generator)];
    set_net_channel(channel);
}

void StaticChannelStrategy::do_something() {}

void StaticRandomChannelStrategy::do_something() {
    netchannel = stoi(exec("iw dev " + netinterface + " info | grep channel | awk '{print $3}' | tr -d '('"));

    if (target_netchannel == 0) {
        std::uniform_int_distribution<int> int_dist(0, possible_channels.size() - 1);
        target_netchannel = possible_channels[int_dist(generator)];
        std::cout << "\n\033[42msetting target netchannel to " + std::to_string(target_netchannel) + "\033[0m\n";
    }

    if (netchannel != target_netchannel) {
        set_net_channel(target_netchannel);
    }
}
