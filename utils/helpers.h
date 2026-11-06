#pragma once
#include <string>

// The assignment's home is the launch directory, independent of $HOME.
void setShellHome();
const std::string& shellHome();
std::string currentDirectory();
std::string abbreviateHome(const std::string& path);
std::string expandHome(const std::string& path);
std::string getFormattedCWD();
bool parseNonnegativeInt(const std::string& text, int& value);
