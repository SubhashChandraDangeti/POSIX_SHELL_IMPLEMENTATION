#include "builtins/history.h"
#include "system/system.h"
#include "utils/completion.h"
#include "utils/helpers.h"
#include "utils/parser.h"
#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <limits.h>
#include <pwd.h>
#include <readline/readline.h>
#include <stdexcept>
#include <unistd.h>

namespace {
Executor* activeExecutor = nullptr;

int onReadlineEvent() {
    if (consumePromptInterrupt()) {
        if (activeExecutor) activeExecutor->setLastStatus(130);
        rl_replace_line("", 0);
        rl_crlf();
        rl_on_new_line();
        rl_redisplay();
    }
    if (activeExecutor && activeExecutor->reapBackground()) {
        rl_on_new_line();
        rl_redisplay();
    }
    return 0;
}

std::string prompt() {
    const auto* account = getpwuid(geteuid());
    const std::string user = account ? account->pw_name : std::to_string(geteuid());
    char hostname[HOST_NAME_MAX + 1] = {};
    if (gethostname(hostname, HOST_NAME_MAX) < 0) return "<" + user + "@unknown:" + getFormattedCWD() + "> ";
    return "<" + user + "@" + hostname + ":" + getFormattedCWD() + "> ";
}

bool readBatchLine(std::string& line) {
    // Read exactly one line. Buffered iostream reads could steal stdin bytes
    // that the next external command (for example cat) should receive.
    for (;;) {
        char byte;
        const auto length = read(STDIN_FILENO, &byte, 1);
        if (length < 0 && errno == EINTR) continue;
        if (length < 0) throw std::runtime_error("cannot read command input");
        if (length == 0) return !line.empty();
        if (byte == '\n') return true;
        line += byte;
    }
}
}

int main() {
    try {
        setShellHome();
        loadHistory();
        const bool interactive = isatty(STDIN_FILENO);
        Executor executor(interactive);
        activeExecutor = &executor;
        if (interactive) {
            // The executor owns signal behavior; readline owns editing only.
            rl_catch_signals = 0;
            rl_event_hook = onReadlineEvent;
            initializeCompletion();
        }
        while (!executor.exiting()) {
            executor.reapBackground();
            std::string line;
            if (interactive) {
                char* input = readline(prompt().c_str());
                if (!input) { std::cout << '\n'; break; }
                line = input;
                free(input);
            } else if (!readBatchLine(line)) break;
            addToHistory(line);
            try {
                for (const auto& pipeline : parseLine(line)) {
                    executor.execute(pipeline);
                    if (executor.exiting()) break;
                }
            } catch (const std::exception& error) {
                std::cerr << "myshell: " << error.what() << '\n';
                executor.setLastStatus(2);
            }
        }
        activeExecutor = nullptr;
        rl_event_hook = nullptr;
        return executor.exitStatus();
    } catch (const std::exception& error) {
        std::cerr << "myshell: " << error.what() << '\n';
        return 1;
    }
}
