#include "history.h"
#include "../utils/helpers.h"
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <fstream>
#include <iostream>
#include <readline/history.h>
#include <fcntl.h>
#include <unistd.h>

namespace {
constexpr std::size_t capacity = 20;
std::deque<std::string> commands;
std::string historyPath() {
    if (const char* path = std::getenv("MYSHELL_HISTORY")) return path;
    if (const char* home = std::getenv("HOME")) return std::string(home) + "/.my_shell_history";
    return shellHome() + "/.my_shell_history";
}
}

void loadHistory() {
    commands.clear();
    clear_history();
    stifle_history(static_cast<int>(capacity));
    std::ifstream file(historyPath());
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        commands.push_back(line);
        if (commands.size() > capacity) commands.pop_front();
    }
    for (const auto& command : commands) add_history(command.c_str());
}

void saveHistory() {
    // Write completely before renaming: an interrupted save leaves the old file intact.
    const auto destination = historyPath();
    std::string temporary = destination + ".tmp.XXXXXX";
    const int fd = mkstemp(temporary.data());
    if (fd < 0) { perror("history: create temporary file"); return; }
    bool success = true;
    for (const auto& command : commands) {
        const auto line = command + '\n';
        std::size_t offset = 0;
        while (offset < line.size()) {
            const auto written = write(fd, line.data() + offset, line.size() - offset);
            if (written < 0 && errno == EINTR) continue;
            if (written <= 0) { success = false; break; }
            offset += static_cast<std::size_t>(written);
        }
        if (!success) break;
    }
    if (close(fd) < 0) success = false;
    if (success && rename(temporary.c_str(), destination.c_str()) == 0) return;
    perror("history: save");
    unlink(temporary.c_str());
}

void addToHistory(const std::string& command) {
    if (command.find_first_not_of(" \t\r\n") == std::string::npos ||
        (!commands.empty() && commands.back() == command)) return;
    commands.push_back(command);
    if (commands.size() > capacity) commands.pop_front();
    add_history(command.c_str());
    saveHistory();
}

int runHistory(const std::vector<std::string>& args) {
    int count = 10;
    if (args.size() > 2 || (args.size() == 2 && !parseNonnegativeInt(args[1], count))) {
        std::cerr << "usage: history [nonnegative-count]\n";
        return 1;
    }
    count = std::min(count, static_cast<int>(commands.size()));
    for (auto i = commands.size() - count; i < commands.size(); ++i)
        std::cout << commands[i] << '\n';
    return 0;
}
