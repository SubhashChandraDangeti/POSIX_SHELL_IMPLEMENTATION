#include <bits/stdc++.h>
#include <unistd.h>
#include <sys/wait.h>
#include <csignal>
#include "system.h"
using namespace std;

void handle_sigchld(int sig) {
    int status;
    pid_t pid;
    // Reap *all* finished children
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        cout << "\nBackground process with PID " << pid << " exited." << endl;
    }
}
void runSystemCommand(vector<string>& args) {
    if (args.empty()) return;

    // Check for background process
    bool background = false;
    if (args.back() == "&") {
        background = true;
        args.pop_back(); // remove "&" from args
    }

    // Convert vector<string> to char*[]
    vector<char*> argv;
    for (auto &s : args) argv.push_back((char*)s.c_str());
    argv.push_back(NULL);

    pid_t pid = fork();

    if (pid < 0) {
        perror("Fork failed");
        return;
    }
    else if (pid == 0) {
        // Child process
        if (execvp(argv[0], argv.data()) == -1) {
            perror("Error executing command");
            exit(EXIT_FAILURE);
        }
    }
    else {
        // Parent process
        if (background) {
            cout << "Started background process with PID " << pid << endl;
        } else {
            int status;
            waitpid(pid, &status, 0);
        }
    }
}
