#pragma once

#include <string>

// global flags from command line arguments
extern int verbosity;
extern bool fileoutput;

void rtrim(std::string &s);

// execute system command and return stdout as string
std::string exec(const std::string cmd);
