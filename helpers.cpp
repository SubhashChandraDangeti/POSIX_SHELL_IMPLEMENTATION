#include "helpers.h"
#include <iostream>
#include <unistd.h>
#include <limits.h>
#include <cstring>
#include <sstream>

#include <algorithm>
#include <cctype>

using namespace std;

extern string shellHomeDir;

void setShellHome() {
    char path[PATH_MAX];
    if (getcwd(path, sizeof(path)) != nullptr) {
        shellHomeDir = path;
    } else {
        perror("getcwd");
    }
}

string getFormattedCWD() {
    char currentPath[PATH_MAX];
    if (getcwd(currentPath, sizeof(currentPath)) == nullptr) {
        return "error";
    }
    string currentPathStr(currentPath);
    if (currentPathStr.rfind(shellHomeDir, 0) == 0) {
        return "~" + currentPathStr.substr(shellHomeDir.length());
    } else {
        return currentPathStr;
    }
}


std::vector<std::string> splitCommand(const std::string &s, const std::string &delimiter) {
    std::vector<std::string> tokens;

    if (delimiter == " ") {
        // split by whitespace
        std::istringstream stream(s);
        std::string token;
        while (stream >> token) {
            tokens.push_back(token);
        }
    } else {
        // split by other delimiter like ";"
        size_t start = 0, end = 0;
        while ((end = s.find(delimiter, start)) != std::string::npos) {
            std::string token = s.substr(start, end - start);
            if (!token.empty()) tokens.push_back(token);
            start = end + delimiter.size();
        }
        std::string token = s.substr(start);
        if (!token.empty()) tokens.push_back(token);
    }

    return tokens;
}

void trimSpaces(string& str) {
    while (!str.empty() && isspace(str.front())) str.erase(str.begin());
    while (!str.empty() && isspace(str.back())) str.pop_back();
}
