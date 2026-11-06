// Linux end-to-end tests. No shell/system() wrapper: launch the binary directly.
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <poll.h>
#include <pty.h>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {
using Clock = std::chrono::steady_clock;
std::string binary;
int checks = 0;

void check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
    ++checks;
}

void writeAll(int fd, const std::string& text) {
    std::size_t offset = 0;
    while (offset < text.size()) {
        const auto result = write(fd, text.data() + offset, text.size() - offset);
        if (result < 0 && errno == EINTR) continue;
        if (result <= 0) throw std::runtime_error("write failed");
        offset += static_cast<std::size_t>(result);
    }
}

void removeTree(const std::string& path) {
    DIR* dir = opendir(path.c_str());
    if (!dir) { unlink(path.c_str()); return; }
    while (auto* entry = readdir(dir)) {
        const std::string name = entry->d_name;
        if (name == "." || name == "..") continue;
        const auto child = path + "/" + name;
        struct stat info{};
        if (lstat(child.c_str(), &info) == 0 && S_ISDIR(info.st_mode)) removeTree(child);
        else unlink(child.c_str());
    }
    closedir(dir);
    rmdir(path.c_str());
}

struct Sandbox {
    std::string path;
    Sandbox() {
        char pattern[] = "/tmp/myshell-test-XXXXXX";
        const char* created = mkdtemp(pattern);
        if (!created) throw std::runtime_error("mkdtemp failed");
        path = created;
    }
    ~Sandbox() { removeTree(path); }
    void file(const std::string& name, const std::string& contents) const {
        std::ofstream stream(path + "/" + name);
        stream << contents;
        if (!stream) throw std::runtime_error("cannot create fixture");
    }
    std::string read(const std::string& name) const {
        std::ifstream stream(path + "/" + name);
        return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    }
};

void configureChild(const Sandbox& box) {
    if (chdir(box.path.c_str()) < 0) _exit(120);
    setenv("MYSHELL_HISTORY", (box.path + "/history-store").c_str(), 1);
    setenv("TERM", "dumb", 1);
    setenv("LC_ALL", "C", 1);
    umask(0022);
}

struct Result { int code; std::string output; };
Result run(const Sandbox& box, const std::string& script) {
    int input[2], output[2];
    if (pipe(input) < 0 || pipe(output) < 0) throw std::runtime_error("pipe failed");
    const pid_t pid = fork();
    if (pid < 0) throw std::runtime_error("fork failed");
    if (pid == 0) {
        setsid();
        configureChild(box);
        dup2(input[0], STDIN_FILENO);
        dup2(output[1], STDOUT_FILENO);
        dup2(output[1], STDERR_FILENO);
        close(input[0]); close(input[1]); close(output[0]); close(output[1]);
        execl(binary.c_str(), binary.c_str(), nullptr);
        _exit(121);
    }
    close(input[0]); close(output[1]);
    writeAll(input[1], script);
    close(input[1]);
    std::string text;
    const auto deadline = Clock::now() + std::chrono::seconds(10);
    for (;;) {
        if (Clock::now() >= deadline) {
            kill(pid, SIGKILL);
            waitpid(pid, nullptr, 0);
            close(output[0]);
            throw std::runtime_error("batch timeout: " + script.substr(0, 80));
        }
        pollfd event{output[0], POLLIN, 0};
        if (poll(&event, 1, 100) < 0 && errno != EINTR) throw std::runtime_error("poll failed");
        if (!(event.revents & (POLLIN | POLLHUP))) continue;
        char buffer[4096];
        const auto length = read(output[0], buffer, sizeof(buffer));
        if (length == 0) break;
        if (length < 0) { if (errno == EINTR) continue; throw std::runtime_error("read failed"); }
        text.append(buffer, length);
    }
    close(output[0]);
    int status = 0;
    waitpid(pid, &status, 0);
    return {WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status), text};
}

void batchTests() {
    Sandbox box;
    auto result = run(box, "echo one;echo 'two words';echo a\"b c\"; echo '' tail\npwd\nexit\n");
    check(result.code == 0 && result.output == "one\ntwo words\nab c\n tail\n" + box.path + "\n", "built-ins, quotes, absolute pwd: " + result.output);
    result = run(box, "cat\nexternal-input\n");
    check(result.code == 0 && result.output == "external-input\n", "batch reader does not consume an external command's stdin");

    mkdir((box.path + "/nested").c_str(), 0755);
    result = run(box, "cd nested; pwd; cd ~; pwd; cd -; pwd; cd ~/nested; pwd\n");
    check(result.code == 0 && result.output == box.path + "/nested\n" + box.path + "\n" +
          box.path + "/nested\n" + box.path + "/nested\n" + box.path + "/nested\n", "cd state and launch home: " + result.output);

    result = run(box, "echo alpha>out; echo beta>>out;cat<out|tr a-z A-Z>upper;cat upper\necho restored\n");
    check(result.code == 0 && result.output == "ALPHA\nBETA\nrestored\n", "redirection, pipes, and stream restoration: " + result.output);
    check(box.read("out") == "alpha\nbeta\n", "append preserves content");
    struct stat info{};
    check(stat((box.path + "/out").c_str(), &info) == 0 && (info.st_mode & 0777) == 0644, "output mode is 0644 under umask 022");

    result = run(box, "echo changed >first >second; echo discard >out; cat out;cat second;echo visible\n");
    check(result.output == "discard\nchanged\nvisible\n" && box.read("first").empty(), "ordered redirects and truncation");
    result = run(box, "echo spaced > 'two words'; cat < 'two words'\ncd nested >cd-output; pwd; cd ~\n");
    check(result.output == "spaced\n" + box.path + "/nested\n" && box.read("cd-output").empty(), "quoted paths and parent builtin state with redirection");
    result = run(box, "echo fail > bad/path\necho survived\ncat <missing\necho continued\n");
    check(result.output.find("survived\n") != std::string::npos && result.output.find("continued\n") != std::string::npos, "failed redirect restores parent streams");

    result = run(box, "seq 1 200000|cat|wc -l\nyes x|head -n 1\n");
    check(result.code == 0 && result.output == "200000\nx\n", "large pipes run concurrently and close descriptors: " + result.output);
    result = run(box, "pwd|cat\ncd nested|cat\npwd\nexit 9|cat\necho alive\n");
    check(result.code == 0 && result.output == box.path + "\n" + box.path + "\nalive\n", "pipeline built-ins run in child copies");

    box.file("nested/needle", "found\n");
    symlink("..", (box.path + "/nested/loop").c_str());
    result = run(box, "search needle\nsearch nested\nsearch not-found\necho survived\n");
    check(result.output == "True\nTrue\nFalse\nsurvived\n", "recursive search avoids symlink loops");
    result = run(box, "ls nested/needle\nls -l nested/loop\nls -z\necho survived\n");
    check(result.output.find("nested/needle\n") == 0 && result.output.find("nested/loop -> ..") != std::string::npos && result.output.find("survived\n") != std::string::npos, "ls file, symlink, and option errors");
    result = run(box, "ls -al nested >builtin-ls\n/usr/bin/ls -al --color=never nested >system-ls\n");
    check(result.code == 0 && box.read("builtin-ls") == box.read("system-ls"), "ls -al content matches system ls on fixture");
    result = run(box, "ls ~ >home-list\n");
    check(result.code == 0 && box.read("home-list").find("nested\n") != std::string::npos, "ls tilde uses launch directory");

    result = run(box, "history garbage\npinfo nope\npinfo 999999999999999999999\nfg ''\nbg %\nexit invalid\necho survived\n");
    check(result.code == 0 && result.output.find("survived\n") != std::string::npos && result.output.find("usage: pinfo") != std::string::npos, "invalid numeric arguments cannot crash shell");
    result = run(box, "pinfo\npinfo 2147483647\n");
    check(result.code == 1 && result.output.find("Process Status -- ") != std::string::npos && result.output.find("kB {Virtual Memory}") != std::string::npos && result.output.find("Executable Path -- " + binary) != std::string::npos, "pinfo memory units, executable path, and missing processes");
    result = run(box, "not-a-real-command-for-test\n");
    check(result.code == 127, "unknown executable returns 127");
    result = run(box, "echo should-not-run; echo |\necho survived\n");
    check(result.output.find("should-not-run\n") == std::string::npos && result.output.find("survived\n") != std::string::npos, "syntax error rejects whole line and shell recovers");
    result = run(box, "echo |\nexit\n");
    check(result.code == 2, "exit without a number preserves syntax-error status");
    result = run(box, "exit 7\necho unreachable\n");
    check(result.code == 7 && result.output.empty(), "explicit exit status");

    // Idle notifications are tested with a PTY below. Here exercise heavy churn.
    std::string churn;
    for (int i = 0; i < 80; ++i) churn += "true &\n";
    churn += "sleep 0.2\njobs\necho complete\n";
    result = run(box, churn);
    std::size_t done = 0, position = 0;
    while ((position = result.output.find("] Done ", position)) != std::string::npos) { ++done; ++position; }
    check(result.code == 0 && done == 80 && result.output.find("complete\n") != std::string::npos, "reap all background children without stealing foreground statuses");
    result = run(box, "sleep 0.1|cat & sleep 0.2\njobs\n");
    check(result.code == 0 && result.output.find("Done sleep 0.1 | cat") != std::string::npos, "background pipeline tracked as one job");

    Sandbox history;
    std::string commands;
    for (int i = 0; i < 25; ++i) commands += "echo item" + std::to_string(i) + "\n";
    run(history, commands);
    const auto stored = history.read("history-store");
    check(std::count(stored.begin(), stored.end(), '\n') == 20 && stored.find("echo item5\n") == 0, "history retains latest twenty");
    result = run(history, "history 3\n");
    check(result.output == "echo item23\necho item24\nhistory 3\n", "history persists across sessions and includes current command");
    result = run(history, "history\n");
    check(std::count(result.output.begin(), result.output.end(), '\n') == 10, "history defaults to ten entries");
    run(history, "echo repeated\necho repeated\n");
    const auto deduplicated = history.read("history-store");
    const auto first = deduplicated.find("echo repeated\n");
    check(first != std::string::npos && deduplicated.find("echo repeated\n", first + 1) == std::string::npos, "consecutive history duplicates suppressed");
    check(stat((history.path + "/history-store").c_str(), &info) == 0 && (info.st_mode & 0777) == 0600, "history file is private");
    Sandbox longHistory;
    commands.clear();
    const std::string longWord(300, 'x');
    for (int i = 0; i < 20; ++i) commands += "echo " + longWord + std::to_string(i) + "\n";
    run(longHistory, commands);
    result = run(longHistory, "history 2\n");
    check(result.output == "echo " + longWord + "19\nhistory 2\n", "history reload is not truncated at 4096 bytes");
}

class Terminal {
public:
    explicit Terminal(const Sandbox& box) {
        pid_ = forkpty(&master_, nullptr, nullptr, nullptr);
        if (pid_ < 0) throw std::runtime_error("forkpty failed");
        if (pid_ == 0) {
            configureChild(box);
            execl(binary.c_str(), binary.c_str(), nullptr);
            _exit(121);
        }
        until("> ");
    }
    ~Terminal() {
        if (pid_ > 0) {
            const pid_t foreground = tcgetpgrp(master_);
            if (foreground > 0 && foreground != pid_) kill(-foreground, SIGKILL);
            kill(pid_, SIGKILL);
            waitpid(pid_, nullptr, 0);
        }
        close(master_);
    }
    void send(const std::string& text) { writeAll(master_, text); }
    std::string until(const std::string& marker) {
        const auto deadline = Clock::now() + std::chrono::seconds(5);
        while (pending_.find(marker) == std::string::npos) {
            if (Clock::now() > deadline) throw std::runtime_error("PTY timeout waiting for " + marker + ": " + pending_);
            pollfd event{master_, POLLIN, 0};
            poll(&event, 1, 100);
            if (!(event.revents & POLLIN)) continue;
            char buffer[4096];
            const auto size = read(master_, buffer, sizeof(buffer));
            if (size <= 0) throw std::runtime_error("PTY closed: " + pending_);
            pending_.append(buffer, size);
        }
        const auto end = pending_.find(marker) + marker.size();
        auto result = pending_.substr(0, end);
        pending_.erase(0, end);
        return result;
    }
    void command(const std::string& text) { send(text + '\n'); }
    void waitForJob() {
        const auto deadline = Clock::now() + std::chrono::seconds(3);
        while (tcgetpgrp(master_) == pid_) {
            if (Clock::now() >= deadline) throw std::runtime_error("foreground job did not acquire terminal");
            poll(nullptr, 0, 10);
        }
    }
    void eof() {
        send(std::string(1, '\004'));
        const auto deadline = Clock::now() + std::chrono::seconds(3);
        int status = 0;
        while (waitpid(pid_, &status, WNOHANG) == 0) {
            if (Clock::now() > deadline) throw std::runtime_error("Ctrl-D did not exit");
            poll(nullptr, 0, 10);
        }
        pid_ = -1;
        check(WIFEXITED(status) && WEXITSTATUS(status) == 0, "Ctrl-D exits cleanly");
    }
private:
    int master_ = -1;
    pid_t pid_ = -1;
    std::string pending_;
};

void interactiveTests() {
    Sandbox box;
    box.file("alpha-unique.txt", "completion-ok\n");
    mkdir((box.path + "/folder-unique").c_str(), 0755);
    run(box, "echo persisted-arrow\n");
    Terminal terminal(box);
    terminal.send("\033[A\n");
    auto output = terminal.until("> ");
    check(output.find("persisted-arrow\r\n") != std::string::npos, "Up arrow recalls persisted history");
    terminal.command("echo navigation-first"); terminal.until("> ");
    terminal.command("echo navigation-second"); terminal.until("> ");
    terminal.send("\033[A\033[A\033[B\n");
    output = terminal.until("> ");
    check(output.find("navigation-second\r\n") != std::string::npos, "Down arrow recalls newer command");

    terminal.send("ech\tcommand-complete\n");
    output = terminal.until("> ");
    check(output.find("command-complete\r\n") != std::string::npos && output.find("No such file") == std::string::npos, "builtin command completion: " + output);
    terminal.send("cat alpha-\t\n");
    output = terminal.until("> ");
    check(output.find("completion-ok\r\n") != std::string::npos, "filename completion");
    terminal.send("cd folder-\t\n");
    output = terminal.until("> ");
    check(output.find("~/folder-unique") != std::string::npos, "directory completion");
    terminal.command("cd ~"); terminal.until("> ");
    terminal.send("cat ~/alpha-\t\n");
    output = terminal.until("> ");
    check(output.find("completion-ok\r\n") != std::string::npos, "completion expands launch-directory tilde: " + output);
    terminal.send("echo one; ech\tafter-separator\n");
    output = terminal.until("> ");
    check(output.find("after-separator\r\n") != std::string::npos, "command completion after semicolon");

    terminal.command("cat > terminal-input");
    terminal.waitForJob();
    terminal.send("terminal-read-ok\n");
    terminal.until("terminal-read-ok\r\n");
    terminal.send(std::string(1, '\004'));
    terminal.until("> ");
    check(box.read("terminal-input") == "terminal-read-ok\n", "foreground child reads terminal after handoff");

    terminal.command(binary);
    terminal.until("> ");
    terminal.command("echo nested-ok");
    output = terminal.until("> ");
    check(output.find("nested-ok\r\n") != std::string::npos, "nested shell acquires terminal");
    terminal.send(std::string(1, '\004'));
    terminal.until("> ");
    terminal.command("echo parent-returned");
    output = terminal.until("> ");
    check(output.find("parent-returned\r\n") != std::string::npos, "parent restores terminal after nested shell exits");

    terminal.command("pinfo");
    output = terminal.until("> ");
    check(output.find("+\r\n") != std::string::npos, "pinfo marks foreground group");
    terminal.command("sleep 0.15 &"); terminal.until("> ");
    terminal.until("Done sleep 0.15"); terminal.until("> ");
    check(true, "background process reaped while prompt is idle");

    terminal.command("sleep 0.2 &");
    output = terminal.until("> ");
    const auto bracket = output.find(']');
    check(bracket != std::string::npos, "background launch prints PID");
    const auto background = std::strtol(output.c_str() + bracket + 1, nullptr, 10);
    terminal.command("sleep 30");
    terminal.waitForJob();
    const auto reapDeadline = Clock::now() + std::chrono::seconds(2);
    const auto processPath = "/proc/" + std::to_string(background) + "/stat";
    while (access(processPath.c_str(), F_OK) == 0 && Clock::now() < reapDeadline)
        poll(nullptr, 0, 20);
    check(background > 0 && access(processPath.c_str(), F_OK) < 0, "background child reaped during foreground wait");
    terminal.send(std::string(1, '\003'));
    terminal.until("> ");

    terminal.command("sleep 30 | cat");
    terminal.waitForJob();
    terminal.send(std::string(1, '\032'));
    output = terminal.until("> ");
    check(output.find("Stopped") != std::string::npos, "Ctrl-Z stops entire foreground pipeline");
    terminal.command("jobs");
    output = terminal.until("> ");
    check(output.find("Stopped") != std::string::npos && output.find("sleep 30 | cat") != std::string::npos, "stopped job recorded");
    terminal.command("bg"); terminal.until("> ");
    terminal.command("jobs");
    output = terminal.until("> ");
    check(output.find("Running") != std::string::npos, "bg resumes stopped pipeline");
    terminal.command("fg");
    terminal.waitForJob();
    terminal.send(std::string(1, '\003'));
    terminal.until("> ");
    terminal.command("echo after-interrupt");
    output = terminal.until("> ");
    check(output.find("after-interrupt\r\n") != std::string::npos, "Ctrl-C interrupts job and shell survives");
    terminal.send("echo must-not-execute");
    terminal.send(std::string(1, '\003'));
    terminal.until("> ");
    terminal.command("echo clean-prompt");
    output = terminal.until("> ");
    check(output.find("clean-prompt\r\n") != std::string::npos, "Ctrl-C clears prompt input");
    terminal.send(std::string(1, '\032'));
    terminal.command("echo shell-still-running");
    output = terminal.until("> ");
    check(output.find("shell-still-running\r\n") != std::string::npos, "Ctrl-Z at prompt does not suspend shell");
    terminal.eof();
}
}

int main(int argc, char** argv) {
    try {
        if (argc < 2) throw std::runtime_error("usage: integration_test <shell-path> [--interactive]");
        char* path = realpath(argv[1], nullptr);
        if (!path) throw std::runtime_error("cannot resolve shell binary");
        binary = path;
        free(path);
        if (argc == 3 && std::string(argv[2]) == "--interactive") interactiveTests();
        else batchTests();
        std::cout << "PASS: " << checks << " " << (argc == 3 ? "terminal" : "integration") << " checks\n";
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
