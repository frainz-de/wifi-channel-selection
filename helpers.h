#pragma once

#include <string>
#include <vector>

// global flags from command line arguments
extern int verbosity;
extern bool fileoutput;
extern int random_seed;
extern std::vector<int> possible_channels;

void rtrim(std::string &s);

// execute system command and return stdout as string
std::string exec(const std::string cmd);
