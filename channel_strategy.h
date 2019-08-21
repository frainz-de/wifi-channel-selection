#pragma once

#include <string>
#include <chrono>
#include <vector>
#include <map>
#include <tuple>

class NeighborManager;
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
    void record_channel(std::string address, int freq);

protected:
    NeighborManager* neighbor_manager;
    //std::vector<int> possible_channels = {5170,5190,5230,5270,5310,5510,5550,5590,5630,5670,5710,5755,5795};
    std::vector<int> possible_channels = {5180,5200,5220,5240,5745,5765,5785,5805,5825};
    std::string specinterface;
    std::string netinterface;
    int specchannel;
    int netchannel;

    void switch_channel(int freq);
    void set_spec_channel(int freq);
    int get_least_used_channel();

    // map of address and tuple of correlation and timestamp
    std::map<std::string, std::tuple<double, std::chrono::time_point<Clock>>> correlations;
    std::map<std::string, int> channels; // map of neighbor and channel
private:
};

class CorrelationChannelStrategy: public ChannelStrategy {
public:
    using ChannelStrategy::ChannelStrategy; // inherit constructors from base class
    virtual void do_something();

};

class RandomChannelStrategy: public ChannelStrategy {
public:
    using ChannelStrategy::ChannelStrategy; // inherit constructors from base class
    virtual void do_something();

private:
    std::chrono::time_point<Clock> last_checked;
};
