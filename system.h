#pragma once
#include <vector>
#include <string>
using namespace std;

// Function to execute system commands (foreground & background)
void runSystemCommand(vector<string>& args);
// Signal handler for reaping background processes
void handle_sigchld(int sig);
