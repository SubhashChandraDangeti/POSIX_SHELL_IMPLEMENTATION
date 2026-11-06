#include "../utils/parser.h"
#include "../utils/helpers.h"
#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {
void check(bool condition, const char* message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
}

int main() {
    setShellHome();
    auto line = parseLine("\t echo hello>out; cat<in|wc -l>>count & pwd");
    check(line.size() == 3, "separators without spaces");
    check(line[0].commands[0].args == std::vector<std::string>({"echo", "hello"}), "redirection removed from argv");
    check(line[0].commands[0].redirects[0].mode == RedirectMode::Truncate, "truncate mode");
    check(line[1].background && line[1].commands.size() == 2, "background pipeline");
    check(line[1].commands[0].redirects[0].mode == RedirectMode::Input, "input mode");
    check(line[1].commands[1].redirects[0].mode == RedirectMode::Append, "append mode");
    auto quoted = parseLine("echo '' a\" b\"' c' 'x;|&<>' escaped\\ word\tend");
    check(quoted[0].commands[0].args == std::vector<std::string>({"echo", "", "a b c", "x;|&<>", "escaped word", "end"}), "quotes, empty words, and escapes");
    check(parseLine(" ; ; \t").empty(), "blank commands");
    auto tilde = parseLine("echo ~ ~/child '~' \\~");
    check(tilde[0].commands[0].args == std::vector<std::string>({"echo", shellHome(), shellHome() + "/child", "~", "~"}), "only unquoted, unescaped leading tilde expands");
    auto ordered = parseLine("echo > first > second hi");
    check(ordered[0].commands[0].redirects.size() == 2, "ordered redirects");
    for (const auto* input : {"| wc", "echo |", "echo || wc", "echo >", "echo < | wc", "&", "echo && pwd", "echo 'oops", "echo \\", "''"}) {
        bool rejected = false;
        try { parseLine(input); } catch (const std::runtime_error&) { rejected = true; }
        check(rejected, input);
    }
    std::cout << "PASS: parser cases\n";
}
