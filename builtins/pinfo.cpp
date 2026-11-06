#include "pinfo.h"
#include "../utils/helpers.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <limits.h>
#include <unistd.h>

int runPinfo(const std::vector<std::string>& args) {
    int pid = getpid();
    if (args.size() > 2 || (args.size() == 2 &&
        (!parseNonnegativeInt(args[1], pid) || pid == 0))) {
        std::cerr << "usage: pinfo [positive-pid]\n";
        return 1;
    }
    const auto base = "/proc/" + std::to_string(pid);
    std::ifstream file(base + "/stat");
    std::string line;
    if (!std::getline(file, line)) {
        std::cerr << "pinfo: cannot read process " << pid << '\n';
        return 1;
    }
    // comm (field 2) may contain spaces or ')': numeric fields follow its LAST ')'.
    const auto end = line.rfind(')');
    if (end == std::string::npos) {
        std::cerr << "pinfo: malformed process data\n";
        return 1;
    }
    std::istringstream fields(line.substr(end + 1));
    char state;
    long parent, group, session, tty, foreground;
    if (!(fields >> state >> parent >> group >> session >> tty >> foreground)) {
        std::cerr << "pinfo: incomplete process data\n";
        return 1;
    }
    std::ifstream status(base + "/status");
    std::string memory = "unavailable";
    while (std::getline(status, line)) {
        if (line.compare(0, 7, "VmSize:") == 0) {
            std::istringstream value(line.substr(7));
            std::string amount, unit;
            value >> amount >> unit;
            memory = amount + " " + unit;
            break;
        }
    }
    char executable[PATH_MAX + 1];
    const auto length = readlink((base + "/exe").c_str(), executable, PATH_MAX);
    const std::string path = length < 0 ? "unavailable" :
                             abbreviateHome(std::string(executable, length));
    std::cout << "pid -- " << pid << "\nProcess Status -- " << state;
    if (tty != 0 && group == foreground) std::cout << '+';
    std::cout << "\nmemory -- " << memory << " {Virtual Memory}\n"
              << "Executable Path -- " << path << '\n';
    return 0;
}
