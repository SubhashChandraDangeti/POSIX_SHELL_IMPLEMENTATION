#include <bits/stdc++.h>
#include <fcntl.h>   // open
#include <unistd.h>  // read, write, close
#include <sys/stat.h>
#include "history.h"
using namespace std;

const int MAX_HISTORY = 20;
string historyFile = string(getenv("HOME")) + "/.my_shell_history";
deque<string> historyList;

// Load history
void loadHistory() {
    int fd = open(historyFile.c_str(), O_RDONLY);
    if (fd == -1) return; // no history yet

    char buf[4096];
    ssize_t bytesRead = read(fd, buf, sizeof(buf) - 1);
    close(fd);

    if (bytesRead <= 0) return;

    buf[bytesRead] = '\0'; 
    string content(buf);

    stringstream ss(content);
    string line;
    while (getline(ss, line)) {
        if (!line.empty()) {
            historyList.push_back(line);
            if (historyList.size() > MAX_HISTORY)
                historyList.pop_front();
        }
    }
}

// Save history 
void saveHistory() {
    int fd = open(historyFile.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) return;

    for (auto &cmd : historyList) {
        string line = cmd + "\n";
        write(fd, line.c_str(), line.size());
    }

    close(fd);
}

// Add command
void addToHistory(const string &cmd) {
    if (cmd.empty()) return;
    if (!historyList.empty() && historyList.back() == cmd) return; // avoid consecutive duplicates

    historyList.push_back(cmd);
    if (historyList.size() > MAX_HISTORY)
        historyList.pop_front();

    saveHistory();
}

// Run history command
void runHistory(vector<string> &args) {
    int n = 10; // default
    if (args.size() == 2) {
        n = stoi(args[1]);
        n = min(n, (int)historyList.size());
    } else {
        n = min(n, (int)historyList.size());
    }

    int start = historyList.size() - n;
    for (int i = start; i < (int)historyList.size(); i++) {
        cout << historyList[i] << endl;
    }
}
