#include "pwd.h"
#include "../utils/helpers.h"
#include <iostream>

int runPWD() {
    std::cout << currentDirectory() << '\n';
    return 0;
}
