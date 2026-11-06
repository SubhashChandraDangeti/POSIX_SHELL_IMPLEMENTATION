#pragma once
#include <string>
#include <vector>

void loadHistory();
void saveHistory();
void addToHistory(const std::string& command);
int runHistory(const std::vector<std::string>& args);
