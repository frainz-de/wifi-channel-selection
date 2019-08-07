#pragma once

#include <string>
#include <chrono>
#include <vector>

using Clock = std::chrono::system_clock;

class ChannelStrategy {
public:
    //ChannelStrategy() {};
    ChannelStrategy(const std::string& specinterface, const std::string& netinterface);
    virtual ~ChannelStrategy() {};
    //virtual void run(volatile bool* running);
    virtual void do_something() = 0;

protected:
    std::vector<int> possible_channels = {5170,5190,5230,5270,5310,5510,5550,5590,5630,5670,5710,5755,5795};
    std::string specinterface;
    std::string netinterface;
    int specchannel;
    int netchannel;

private:
};

class RandomChannelStrategy: public ChannelStrategy {
public:
    using ChannelStrategy::ChannelStrategy; // inherit constructors from base class
    virtual void do_something();

private:
    std::chrono::time_point<Clock> last_checked;
};
