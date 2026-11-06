#pragma once
#include <string>
#include <vector>

enum class RedirectMode { Input, Truncate, Append };
struct Redirect {
    RedirectMode mode;
    std::string path;
};
struct Command {
    std::vector<std::string> args;
    // Ordered: each redirection takes effect, even if a later one replaces it.
    std::vector<Redirect> redirects;
};
struct Pipeline {
    std::vector<Command> commands;
    bool background = false;
};

// Parse the whole line before executing anything; throws on invalid syntax.
std::vector<Pipeline> parseLine(const std::string& line);
