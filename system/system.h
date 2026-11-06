#pragma once
#include "../utils/parser.h"
#include <string>
#include <sys/types.h>
#include <termios.h>
#include <vector>

bool consumePromptInterrupt();

// One executor owns the terminal and every child it creates.
class Executor {
public:
    explicit Executor(bool interactive);
    ~Executor();
    Executor(const Executor&) = delete;
    Executor& operator=(const Executor&) = delete;
    int execute(const Pipeline& pipeline);
    bool reapBackground();
    bool exiting() const { return exiting_; }
    int exitStatus() const { return exitStatus_; }
    void setLastStatus(int status) { exitStatus_ = status; }

private:
    struct Process {
        pid_t pid;
        bool done = false;
        bool stopped = false;
        int status = 0;
    };
    struct Job {
        int id = 0;
        pid_t group = 0;
        std::string label;
        std::vector<Process> processes;
        termios modes{};
        bool hasModes = false;
    };
    bool interactive_;
    int terminal_ = -1;
    pid_t shellGroup_ = 0;
    termios shellModes_{};
    std::vector<Job> jobs_;
    int nextJob_ = 1;
    bool exiting_ = false;
    int exitStatus_ = 0;

    int executeBuiltin(const std::vector<std::string>& args, bool inChild);
    int launch(const Pipeline& pipeline);
    int waitForeground(Job& job);
    void giveTerminal(const Job& job);
    void restoreTerminal();
    static bool finished(const Job& job);
    static bool stopped(const Job& job);
    static void update(Job& job, pid_t pid, int status);
};
