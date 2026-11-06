#include "parser.h"
#include "helpers.h"
#include <cctype>
#include <cstring>
#include <stdexcept>

namespace {
struct Token {
    std::string text;
    bool word;
    bool expandTilde = false;
};

// The scanner preserves quotes and operators; strtok handles ordinary words.
std::vector<Token> tokenize(const std::string& line) {
    std::vector<Token> tokens;
    std::string word;
    bool started = false;
    bool expandTilde = false;
    auto flush = [&] {
        if (started) tokens.push_back({word, true, expandTilde});
        word.clear();
        started = false;
        expandTilde = false;
    };
    for (std::size_t i = 0; i < line.size();) {
        char c = line[i];
        if (std::isspace(static_cast<unsigned char>(c))) {
            flush();
            ++i;
        } else if (c == '\'' || c == '"') {
            started = true;
            const char quote = c;
            ++i;
            while (i < line.size() && line[i] != quote) word += line[i++];
            if (i == line.size()) throw std::runtime_error("unclosed quote");
            ++i;
        } else if (c == '\\') {
            if (++i == line.size()) throw std::runtime_error("trailing escape");
            started = true;
            word += line[i++];
        } else if (std::strchr(";|&<>", c)) {
            flush();
            std::string op(1, c);
            ++i;
            if (c == '>' && i < line.size() && line[i] == '>') { op += '>'; ++i; }
            tokens.push_back({op, false});
        } else {
            const auto begin = i;
            while (i < line.size() && !std::strchr(";|&<>\"'\\", line[i])) ++i;
            std::string span = line.substr(begin, i - begin);
            const bool trailingSpace = std::isspace(static_cast<unsigned char>(span.back()));
            bool first = true;
            for (char* part = std::strtok(span.data(), " \t\r\n\v\f"); part;
                 part = std::strtok(nullptr, " \t\r\n\v\f")) {
                if (!first) flush();
                if (!started && part[0] == '~') expandTilde = true;
                word += part;
                started = true;
                first = false;
            }
            if (trailingSpace) flush();
        }
    }
    flush();
    return tokens;
}
}

std::vector<Pipeline> parseLine(const std::string& line) {
    auto tokens = tokenize(line);
    std::vector<Pipeline> result;
    std::size_t i = 0;
    while (i < tokens.size()) {
        if (!tokens[i].word && tokens[i].text == ";") { ++i; continue; }
        Pipeline pipeline;
        for (;;) {
            Command command;
            while (i < tokens.size()) {
                const auto& token = tokens[i];
                if (token.word) {
                    command.args.push_back(token.expandTilde ? expandHome(token.text) : token.text);
                    ++i;
                    continue;
                }
                if (token.text != "<" && token.text != ">" && token.text != ">>") break;
                const auto mode = token.text == "<" ? RedirectMode::Input :
                                  token.text == ">" ? RedirectMode::Truncate : RedirectMode::Append;
                ++i;
                if (i == tokens.size() || !tokens[i].word)
                    throw std::runtime_error("redirection requires a filename");
                const auto& path = tokens[i++];
                command.redirects.push_back({mode, path.expandTilde ? expandHome(path.text) : path.text});
            }
            if (command.args.empty() || command.args[0].empty())
                throw std::runtime_error("expected a command");
            pipeline.commands.push_back(std::move(command));
            if (i == tokens.size() || tokens[i].word || tokens[i].text != "|") break;
            ++i;
        }
        if (i < tokens.size() && !tokens[i].word && tokens[i].text == "&") {
            pipeline.background = true;
            ++i;
        } else if (i < tokens.size() && !tokens[i].word && tokens[i].text == ";") {
            ++i;
        } else if (i < tokens.size()) {
            throw std::runtime_error("unexpected operator: " + tokens[i].text);
        }
        result.push_back(std::move(pipeline));
    }
    return result;
}
