#include "pinfo.h"
#include <bits/stdc++.h>
#include <unistd.h>
#include <sys/types.h>
using namespace std;

void runPinfo(vector<string>& args) {
    pid_t pid;

    // 1. Determine PID
    if (args.size() == 1) {
        pid = getpid(); // default: shell’s own PID
    } else {
        pid = stoi(args[1]); // user-provided PID
    }

    // 2. Read status from /proc/<pid>/status
    string statusPath = "/proc/" + to_string(pid) + "/status";
    ifstream statusFile(statusPath);
    if (!statusFile.is_open()) {
        perror("Could not open status file");
        return;
    }
    string line, status;
    while (getline(statusFile, line)) {
        if (line.rfind("State:", 0) == 0) {
            status = line.substr(7); // skip "State:\t"
            break;
        }
    }
    statusFile.close();

    // 3. Read memory from /proc/<pid>/statm
    string memPath = "/proc/" + to_string(pid) + "/statm";
    ifstream memFile(memPath);
    if (!memFile.is_open()) {
        perror("Could not open statm file");
        return;
    }
    string memory;
    memFile >> memory;
    memFile.close();

    // 4. Read executable path from /proc/<pid>/exe
    string exePath = "/proc/" + to_string(pid) + "/exe";
    char exe[1024];
    ssize_t len = readlink(exePath.c_str(), exe, sizeof(exe)-1);
    if (len != -1) exe[len] = '\0';

    // 5. Print
    cout << "Process Status -- " << status << endl;
    cout << "memory -- " << memory << " {Virtual Memory}" << endl;
    cout << "Executable Path -- " << exe << endl;
}
