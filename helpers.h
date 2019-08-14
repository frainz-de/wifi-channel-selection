#pragma once

#include <string>

// global verbosity flag
extern int verbosity;

void rtrim(std::string &s);

// execute system command and return stdout as string
std::string exec(const std::string cmd);
