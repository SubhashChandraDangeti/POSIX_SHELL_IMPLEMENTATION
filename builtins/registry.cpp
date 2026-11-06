#include "registry.h"
#include "cd.h"
#include "echo.h"
#include "history.h"
#include "ls.h"
#include "pinfo.h"
#include "pwd.h"
#include "search.h"
#include <algorithm>
#include <iostream>

const std::vector<std::string>& builtinNames() {
    static const std::vector<std::string> names{
        "cd", "echo", "pwd", "ls", "pinfo", "search", "history", "exit", "jobs", "fg", "bg"};
    return names;
}

bool isBuiltin(const std::string& name) {
    const auto& names = builtinNames();
    return std::find(names.begin(), names.end(), name) != names.end();
}

int runBuiltin(const std::vector<std::string>& args) {
    const auto& name = args[0];
    if (name == "cd") return runCD(args);
    if (name == "echo") return runEcho(args);
    if (name == "ls") return runLS(args);
    if (name == "pinfo") return runPinfo(args);
    if (name == "search") return runSearch(args);
    if (name == "history") return runHistory(args);
    if (name == "pwd") {
        if (args.size() == 1) return runPWD();
        std::cerr << "usage: pwd\n";
        return 1;
    }
    return 1; // exit/jobs/fg/bg require the executor's shell/job state.
}
