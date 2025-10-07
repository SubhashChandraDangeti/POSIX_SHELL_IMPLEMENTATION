#pragma once
#include <string>
#include <vector>

void setShellHome();
std::string getFormattedCWD();

std::vector<std::string> splitCommand(const std::string &s, const std::string &delimiter = " ");

void trimSpaces(std::string& str);
