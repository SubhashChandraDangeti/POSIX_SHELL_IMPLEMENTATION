#include "cd.h"
#include "../utils/helpers.h"
#include <cstdio>
#include <iostream>
#include <unistd.h>

int runCD(const std::vector<std::string>& args) {
    static std::string previous;
    if (args.size() > 2) {
        std::cerr << "Invalid arguments\n";
        return 1;
    }
    const auto current = currentDirectory();
    const bool swap = args.size() == 2 && args[1] == "-";
    if (swap && previous.empty()) {
        std::cerr << "cd: previous directory is not set\n";
        return 1;
    }
    const auto target = swap ? previous :
                        args.size() == 1 ? shellHome() : args[1];
    if (chdir(target.c_str()) < 0) {
        perror(("cd: " + target).c_str());
        return 1;
    }
    previous = current;
    if (swap) std::cout << currentDirectory() << '\n';
    return 0;
}
