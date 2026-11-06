#pragma once
#include <string>
#include <vector>

const std::vector<std::string>& builtinNames();
bool isBuiltin(const std::string& name);
int runBuiltin(const std::vector<std::string>& args);
