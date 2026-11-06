#include "completion.h"
#include "helpers.h"
#include "../builtins/registry.h"
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <readline/readline.h>
#include <set>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace {
std::vector<std::string> candidates;
std::size_t nextCandidate = 0;

char* generate(const char*, int state) {
    if (state == 0) nextCandidate = 0;
    if (nextCandidate == candidates.size()) return nullptr;
    return strdup(candidates[nextCandidate++].c_str());
}

void collectFiles(std::set<std::string>& matches, const std::string& text) {
    const auto slash = text.rfind('/');
    const auto prefix = slash == std::string::npos ? "" : text.substr(0, slash + 1);
    const auto partial = slash == std::string::npos ? text : text.substr(slash + 1);
    const auto directory = prefix.empty() ? "." : expandHome(prefix);
    DIR* dir = opendir(directory.c_str());
    if (!dir) return;
    while (auto* entry = readdir(dir)) {
        const std::string name = entry->d_name;
        if (name == "." || name == ".." || name.compare(0, partial.size(), partial) != 0) continue;
        if (partial.empty() && name[0] == '.') continue;
        struct stat info{};
        const bool isDirectory = stat((directory + "/" + name).c_str(), &info) == 0 && S_ISDIR(info.st_mode);
        matches.insert(prefix + name + (isDirectory ? "/" : ""));
    }
    closedir(dir);
}

void collectCommands(std::set<std::string>& matches, const std::string& text) {
    for (const auto& name : builtinNames())
        if (name.compare(0, text.size(), text) == 0) matches.insert(name);
    const char* env = std::getenv("PATH");
    if (!env) return;
    const std::string path = env;
    std::size_t start = 0;
    for (;;) {
        const auto end = path.find(':', start);
        auto directory = path.substr(start, end == std::string::npos ? end : end - start);
        if (directory.empty()) directory = ".";
        DIR* dir = opendir(directory.c_str());
        if (dir) {
            while (auto* entry = readdir(dir)) {
                const std::string name = entry->d_name;
                if (name.compare(0, text.size(), text) != 0) continue;
                const auto full = directory + "/" + name;
                struct stat info{};
                if (access(full.c_str(), X_OK) == 0 && stat(full.c_str(), &info) == 0 && S_ISREG(info.st_mode))
                    matches.insert(name);
            }
            closedir(dir);
        }
        if (end == std::string::npos) break;
        start = end + 1;
    }
}

char** complete(const char* text, int start, int) {
    rl_attempted_completion_over = 1;
    rl_completion_suppress_append = 0;
    rl_filename_completion_desired = 1;
    rl_filename_quoting_desired = 1;
    int previous = start - 1;
    while (previous >= 0 && (rl_line_buffer[previous] == ' ' || rl_line_buffer[previous] == '\t')) --previous;
    const bool commandPosition = previous < 0 || std::strchr(";|&", rl_line_buffer[previous]);
    std::set<std::string> matches;
    collectFiles(matches, text);
    if (commandPosition && !std::strchr(text, '/')) collectCommands(matches, text);
    candidates.assign(matches.begin(), matches.end());
    char** result = rl_completion_matches(text, generate);
    if (result && result[0]) {
        const std::string common = result[0];
        if (!common.empty() && common.back() == '/') rl_completion_suppress_append = 1;
    }
    return result;
}
}

void initializeCompletion() {
    rl_attempted_completion_function = complete;
    rl_completer_word_break_characters = const_cast<char*>(" \t\n\"'|&;<> ");
}
