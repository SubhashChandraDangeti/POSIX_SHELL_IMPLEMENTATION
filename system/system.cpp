#include "system.h"
#include "../builtins/registry.h"
#include "../utils/helpers.h"
#include <algorithm>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <fcntl.h>
#include <iostream>
#include <stdexcept>
#include <sys/wait.h>
#include <unistd.h>

namespace {
volatile sig_atomic_t promptInterrupted = 0;
volatile sig_atomic_t childrenChanged = 0;

// Handlers never allocate, print, or reap. All work happens in normal control flow.
void interruptHandler(int) { promptInterrupted = 1; }
void childHandler(int) { childrenChanged = 1; }

void setSignal(int number, void (*handler)(int)) {
    struct sigaction action{};
    action.sa_handler = handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESTART;
    if (sigaction(number, &action, nullptr) < 0)
        throw std::runtime_error("sigaction failed");
}

int waitStatus(int status) {
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    if (WIFSTOPPED(status)) return 128 + WSTOPSIG(status);
    return 1;
}

bool applyRedirects(const Command& command) {
    for (const auto& redirect : command.redirects) {
        const bool input = redirect.mode == RedirectMode::Input;
        const int flags = input ? O_RDONLY : O_WRONLY | O_CREAT |
                          (redirect.mode == RedirectMode::Append ? O_APPEND : O_TRUNC);
        const auto& path = redirect.path;
        const int fd = open(path.c_str(), flags, 0644);
        if (fd < 0) { perror(("redirect: " + path).c_str()); return false; }
        const int target = input ? STDIN_FILENO : STDOUT_FILENO;
        if (fd != target) {
            const int result = dup2(fd, target);
            const int error = errno;
            close(fd);
            if (result < 0) { errno = error; perror("dup2"); return false; }
        }
    }
    return true;
}

// Parent-run built-ins must restore the shell's descriptors on every exit path.
class StandardStreams {
public:
    StandardStreams() {
        std::cout.flush();
        std::cerr.flush();
        input_ = dup(STDIN_FILENO);
        output_ = dup(STDOUT_FILENO);
        if (input_ < 0 || output_ < 0) {
            if (input_ >= 0) close(input_);
            if (output_ >= 0) close(output_);
            throw std::runtime_error("cannot save standard descriptors");
        }
    }
    ~StandardStreams() {
        std::cout.flush();
        std::cerr.flush();
        if (dup2(input_, STDIN_FILENO) < 0) perror("restore stdin");
        if (dup2(output_, STDOUT_FILENO) < 0) perror("restore stdout");
        close(input_);
        close(output_);
        std::cin.clear();
        std::cout.clear();
    }
private:
    int input_ = -1, output_ = -1;
};

std::string labelFor(const Pipeline& pipeline) {
    std::string label;
    for (const auto& command : pipeline.commands) {
        if (!label.empty()) label += " | ";
        for (const auto& arg : command.args) {
            if (!label.empty() && label.back() != ' ') label += ' ';
            label += arg;
        }
    }
    return label;
}
}

bool consumePromptInterrupt() {
    if (!promptInterrupted) return false;
    promptInterrupted = 0;
    return true;
}

Executor::Executor(bool interactive) : interactive_(interactive) {
    shellGroup_ = getpgrp();
    if (interactive_) {
        // A nested shell must wait until its parent puts its group in foreground.
        while (tcgetpgrp(STDIN_FILENO) != (shellGroup_ = getpgrp()))
            if (kill(-shellGroup_, SIGTTIN) < 0) throw std::runtime_error("cannot acquire terminal");
        setSignal(SIGTTIN, SIG_IGN);
        setSignal(SIGTTOU, SIG_IGN);
        shellGroup_ = getpid();
        if (setpgid(0, shellGroup_) < 0 && getpgrp() != shellGroup_)
            throw std::runtime_error("cannot create shell process group");
        terminal_ = fcntl(STDIN_FILENO, F_DUPFD_CLOEXEC, 3);
        if (terminal_ < 0 || tcsetpgrp(terminal_, shellGroup_) < 0 ||
            tcgetattr(terminal_, &shellModes_) < 0)
            throw std::runtime_error("cannot initialize terminal");
    }
    setSignal(SIGINT, interruptHandler);
    setSignal(SIGQUIT, SIG_IGN);
    setSignal(SIGTSTP, SIG_IGN);
    setSignal(SIGCHLD, childHandler);
}

Executor::~Executor() {
    // Let stopped jobs process SIGHUP too. Do not wait indefinitely for a job
    // that deliberately ignores it; remaining children are adopted by init.
    for (const auto& job : jobs_) {
        kill(-job.group, SIGHUP);
        kill(-job.group, SIGCONT);
    }
    reapBackground();
    restoreTerminal();
    if (terminal_ >= 0) close(terminal_);
}

bool Executor::finished(const Job& job) {
    return std::all_of(job.processes.begin(), job.processes.end(),
                       [](const Process& process) { return process.done; });
}

bool Executor::stopped(const Job& job) {
    return !finished(job) && std::all_of(job.processes.begin(), job.processes.end(),
                       [](const Process& process) { return process.done || process.stopped; });
}

void Executor::update(Job& job, pid_t pid, int status) {
    for (auto& process : job.processes) {
        if (process.pid != pid) continue;
        process.status = status;
        if (WIFEXITED(status) || WIFSIGNALED(status)) { process.done = true; process.stopped = false; }
        else if (WIFSTOPPED(status)) process.stopped = true;
        else if (WIFCONTINUED(status)) process.stopped = false;
    }
}

bool Executor::reapBackground() {
    if (!childrenChanged && jobs_.empty()) return false;
    childrenChanged = 0;
    bool notified = false;
    // Wait only on registered background children. Never consume a foreground
    // child's status, which belongs exclusively to waitForeground().
    for (auto job = jobs_.begin(); job != jobs_.end();) {
        const bool wasStopped = stopped(*job);
        for (auto& process : job->processes) {
            if (process.done) continue;
            int status = 0;
            pid_t result;
            do {
                result = waitpid(process.pid, &status, WNOHANG | WUNTRACED | WCONTINUED);
                if (result > 0) update(*job, result, status);
            } while (result > 0 || (result < 0 && errno == EINTR));
        }
        if (finished(*job)) {
            std::cout << '\n' << '[' << job->id << "] Done " << job->label << '\n';
            notified = true;
            job = jobs_.erase(job);
        } else {
            if (!wasStopped && stopped(*job)) {
                std::cout << '\n' << '[' << job->id << "] Stopped " << job->label << '\n';
                notified = true;
            }
            ++job;
        }
    }
    if (notified) std::cout.flush();
    return notified;
}

void Executor::giveTerminal(const Job& job) {
    if (!interactive_) return;
    if (tcsetpgrp(terminal_, job.group) < 0) perror("tcsetpgrp: job");
    if (job.hasModes && tcsetattr(terminal_, TCSADRAIN, &job.modes) < 0) perror("tcsetattr: job");
}

void Executor::restoreTerminal() {
    if (!interactive_ || terminal_ < 0) return;
    if (tcsetpgrp(terminal_, shellGroup_) < 0) perror("tcsetpgrp: shell");
    if (tcsetattr(terminal_, TCSADRAIN, &shellModes_) < 0) perror("tcsetattr: shell");
}

int Executor::waitForeground(Job& job) {
    while (!finished(job) && !stopped(job)) {
        int status = 0;
        // Collect background statuses during long foreground waits too. Every
        // status is routed to its owner; notifications wait until the prompt.
        const pid_t pid = waitpid(-1, &status, WUNTRACED | WCONTINUED);
        if (pid < 0) {
            if (errno == EINTR) continue;
            perror("waitpid");
            break;
        }
        const bool foreground = std::any_of(job.processes.begin(), job.processes.end(),
                                           [pid](const Process& process) { return process.pid == pid; });
        if (foreground) update(job, pid, status);
        else for (auto& background : jobs_) update(background, pid, status);
    }
    if (interactive_ && stopped(job)) {
        job.hasModes = tcgetattr(terminal_, &job.modes) == 0;
    }
    restoreTerminal();
    if (stopped(job)) {
        if (!job.id) job.id = nextJob_++;
        std::cout << '\n' << '[' << job.id << "] Stopped " << job.label << '\n';
        jobs_.push_back(job);
    }
    return waitStatus(job.processes.back().status);
}

int Executor::executeBuiltin(const std::vector<std::string>& args, bool inChild) {
    const auto& name = args[0];
    if (name == "exit") {
        int code = exitStatus_;
        if (args.size() > 2 || (args.size() == 2 && !parseNonnegativeInt(args[1], code))) {
            std::cerr << "usage: exit [nonnegative-status]\n";
            return 2;
        }
        if (!inChild) { exiting_ = true; exitStatus_ = code % 256; }
        return code % 256;
    }
    if (name != "jobs" && name != "fg" && name != "bg") return runBuiltin(args);
    if (inChild) {
        std::cerr << name << ": requires a standalone foreground command\n";
        return 1;
    }
    reapBackground();
    if (name == "jobs") {
        if (args.size() != 1) { std::cerr << "usage: jobs\n"; return 1; }
        for (const auto& job : jobs_)
            std::cout << '[' << job.id << "] " << (stopped(job) ? "Stopped " : "Running ")
                      << job.group << ' ' << job.label << '\n';
        return 0;
    }
    int id = jobs_.empty() ? 0 : jobs_.back().id;
    if (args.size() > 2 || (args.size() == 2 &&
        (!parseNonnegativeInt(!args[1].empty() && args[1][0] == '%' ? args[1].substr(1) : args[1], id) || id == 0))) {
        std::cerr << "usage: " << name << " [job-id]\n";
        return 1;
    }
    auto found = std::find_if(jobs_.begin(), jobs_.end(),
                              [id](const Job& job) { return job.id == id; });
    if (found == jobs_.end()) { std::cerr << name << ": no such job\n"; return 1; }
    if (name == "bg") {
        if (kill(-found->group, SIGCONT) < 0) { perror("bg"); return 1; }
        for (auto& process : found->processes) process.stopped = false;
        return 0;
    }
    Job job = *found;
    jobs_.erase(found);
    giveTerminal(job);
    if (kill(-job.group, SIGCONT) < 0 && errno != ESRCH) perror("fg");
    for (auto& process : job.processes) process.stopped = false;
    return waitForeground(job);
}

int Executor::execute(const Pipeline& pipeline) {
    reapBackground();
    const auto& command = pipeline.commands.front();
    int status;
    if (pipeline.commands.size() == 1 && !pipeline.background && isBuiltin(command.args[0])) {
        StandardStreams saved;
        status = applyRedirects(command) ? executeBuiltin(command.args, false) : 1;
    } else status = launch(pipeline);
    if (!exiting_) exitStatus_ = status;
    return status;
}

int Executor::launch(const Pipeline& pipeline) {
    Job job;
    job.label = labelFor(pipeline);
    int gate[2];
    if (pipe(gate) < 0) { perror("pipe: launch gate"); return 1; }
    int previousRead = -1;
    std::cout.flush();
    std::cerr.flush();
    bool failed = false;
    for (std::size_t i = 0; i < pipeline.commands.size(); ++i) {
        const auto& command = pipeline.commands[i];
        const bool last = i + 1 == pipeline.commands.size();
        int next[2] = {-1, -1};
        if (!last && pipe(next) < 0) { perror("pipe"); failed = true; break; }
        const pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            if (next[0] >= 0) { close(next[0]); close(next[1]); }
            failed = true;
            break;
        }
        if (pid == 0) {
            const pid_t group = job.group ? job.group : getpid();
            if (setpgid(0, group) < 0) { perror("setpgid: child"); _exit(1); }
            for (int signal : {SIGINT, SIGQUIT, SIGTSTP, SIGTTIN, SIGTTOU, SIGCHLD, SIGPIPE})
                setSignal(signal, SIG_DFL);
            close(gate[1]);
            char byte;
            // EOF releases every stage only after grouping and terminal handoff.
            while (read(gate[0], &byte, 1) < 0 && errno == EINTR) {}
            close(gate[0]);
            if (terminal_ >= 0) close(terminal_);
            if (previousRead >= 0 && dup2(previousRead, STDIN_FILENO) < 0) {
                perror("dup2: pipe input"); _exit(1);
            }
            if (!last && dup2(next[1], STDOUT_FILENO) < 0) {
                perror("dup2: pipe output"); _exit(1);
            }
            if (previousRead >= 0) close(previousRead);
            if (!last) { close(next[0]); close(next[1]); }
            if (!applyRedirects(command)) _exit(1);
            if (isBuiltin(command.args[0])) {
                int status = 1;
                try { status = executeBuiltin(command.args, true); }
                catch (const std::exception& error) { std::cerr << error.what() << '\n'; }
                std::cout.flush();
                std::cerr.flush();
                _exit(status);
            }
            std::vector<char*> argv;
            for (const auto& arg : command.args) argv.push_back(const_cast<char*>(arg.c_str()));
            argv.push_back(nullptr);
            execvp(argv[0], argv.data());
            const int error = errno;
            perror(argv[0]);
            _exit(error == ENOENT ? 127 : 126);
        }
        if (!job.group) job.group = pid;
        if (setpgid(pid, job.group) < 0 && errno != EACCES && errno != ESRCH) perror("setpgid: parent");
        job.processes.push_back({pid});
        if (previousRead >= 0) close(previousRead);
        if (!last) close(next[1]);
        previousRead = next[0];
    }
    if (previousRead >= 0) close(previousRead);
    close(gate[0]);
    if (failed) {
        if (job.group) kill(-job.group, SIGKILL);
        close(gate[1]);
        for (const auto& process : job.processes)
            while (waitpid(process.pid, nullptr, 0) < 0 && errno == EINTR) {}
        return 1;
    }
    if (!pipeline.background) {
        if (interactive_ && tcgetattr(terminal_, &shellModes_) < 0) perror("tcgetattr: shell");
        giveTerminal(job);
    }
    close(gate[1]);
    if (pipeline.background) {
        job.id = nextJob_++;
        std::cout << '[' << job.id << "] " << job.group << '\n';
        jobs_.push_back(std::move(job));
        return 0;
    }
    return waitForeground(job);
}
