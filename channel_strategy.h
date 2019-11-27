#pragma once

#include <string>
#include <chrono>
#include <vector>
#include <map>
#include <tuple>
#include <random>

class NeighborManager;
class Collector;

using Clock = std::chrono::high_resolution_clock;

class ChannelStrategy {
public:
    //ChannelStrategy() {};
    ChannelStrategy(NeighborManager* neighbor_manager, const std::string& specinterface, const std::string& netinterface);
    virtual ~ChannelStrategy() {};
    //virtual void run(volatile bool* running);
    virtual void do_something() = 0;
    int get_specchannel();
    int get_netchannel();
    void save_correlation(std::string address, double correlation, std::chrono::time_point<Clock> timestamp);
    void save_power_sample();
    void record_channel(std::string address, int freq);
    void set_collector(Collector* collector);
    void print_correlations();
    void print_neighbor_channels();
    void print_power_samples();

protected:
    NeighborManager* neighbor_manager;
    Collector* collector;
    //std::vector<int> possible_channels = {5170,5190,5230,5270,5310,5510,5550,5590,5630,5670,5710,5755,5795};
    //const std::vector<int> possible_channels = {5180,5200,5220,5240,5745,5765,5785,5805,5825};
    std::string specinterface;
    std::string netinterface;
    int specchannel;
    int netchannel;

    void set_net_channel(int freq);
    std::chrono::time_point<Clock> last_net_channel_switch = Clock::now();
    void set_spec_channel(int freq);
    std::chrono::time_point<Clock> last_spec_channel_switch = Clock::now();
    int get_oldest_neighbor_scanchannel();
    std::tuple<int, std::chrono::time_point<Clock>> get_oldest_power_scanchannel();
    bool enough_correlations();

    // map of address and tuple of correlation and timestamp
    std::map<std::string, std::tuple<double, std::chrono::time_point<Clock>>> correlations;
    std::map<std::string, int> neighbor_channel_map; // map of neighbor and channel
    std::map<int, std::tuple<double, std::chrono::time_point<Clock>>> channel_power_map; // channel, power, timestamp

    // random number generation
    std::mt19937_64 generator;
    std::uniform_real_distribution<double> double_dist;

    std::chrono::time_point<Clock> last_checked;
    std::chrono::time_point<Clock> init_time = std::chrono::system_clock::now();
    std::chrono::time_point<Clock> next_change = std::chrono::time_point<Clock>::min();
private:
};

class CorrelationChannelStrategy: public ChannelStrategy {
public:
    using ChannelStrategy::ChannelStrategy; // inherit constructors from base class
    virtual void do_something();
private:
    int pick_channel();
    int pick_scanchannel();
};

class SimpleCorrelationChannelStrategy: public ChannelStrategy {
public:
    using ChannelStrategy::ChannelStrategy; // inherit constructors from base class
    void do_something();
private:
    virtual int get_least_used_channel();
};

class RandomChannelStrategy: public ChannelStrategy {
public:
    using ChannelStrategy::ChannelStrategy; // inherit constructors from base class
    virtual void do_something();

private:
};

class StaticChannelStrategy: public ChannelStrategy {
public:
    using ChannelStrategy::ChannelStrategy; // inherit constructors from base class
    virtual void do_something();
};

class StaticRandomChannelStrategy: public ChannelStrategy {
public:
    using ChannelStrategy::ChannelStrategy; // inherit constructors from base class
    virtual void do_something();

private:
    int target_netchannel = 0;
};
