#pragma once

#include <string>

// global flags from command line arguments
extern int verbosity;
extern bool fileoutput;
extern int random_seed;

void rtrim(std::string &s);

// execute system command and return stdout as string
std::string exec(const std::string cmd);
