#include "echo.h"
#include <iostream>

int runEcho(const std::vector<std::string>& args) {
    for (std::size_t i = 1; i < args.size(); ++i) {
        if (i > 1) std::cout << ' ';
        std::cout << args[i];
    }
    std::cout << '\n';
    return 0;
}
