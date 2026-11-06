#include "helpers.h"
#include <charconv>
#include <cstdlib>
#include <stdexcept>
#include <unistd.h>

namespace {
std::string home;
}

std::string currentDirectory() {
    char* path = getcwd(nullptr, 0);
    if (!path) throw std::runtime_error("getcwd: cannot resolve current directory");
    std::string result(path);
    free(path);
    return result;
}

void setShellHome() { home = currentDirectory(); }
const std::string& shellHome() { return home; }

std::string abbreviateHome(const std::string& path) {
    if (path == home) return "~";
    const std::string prefix = home == "/" ? "/" : home + "/";
    if (path.compare(0, prefix.size(), prefix) == 0)
        return "~/" + path.substr(prefix.size());
    return path;
}

std::string expandHome(const std::string& path) {
    if (path == "~") return home;
    if (path.compare(0, 2, "~/") == 0) return home + path.substr(1);
    return path;
}

std::string getFormattedCWD() { return abbreviateHome(currentDirectory()); }

bool parseNonnegativeInt(const std::string& text, int& value) {
    if (text.empty()) return false;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size() && value >= 0;
}
