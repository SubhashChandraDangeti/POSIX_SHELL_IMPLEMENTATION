#include "search.h"
#include <cstdio>
#include <dirent.h>
#include <iostream>
#include <sys/stat.h>

int runSearch(const std::vector<std::string>& args) {
    if (args.size() != 2) {
        std::cerr << "usage: search <file-or-directory-name>\n";
        return 1;
    }
    std::vector<std::string> pending{"."};
    while (!pending.empty()) {
        const auto path = pending.back();
        pending.pop_back();
        DIR* dir = opendir(path.c_str());
        if (!dir) {
            perror(("search: " + path).c_str());
            continue;
        }
        while (auto* entry = readdir(dir)) {
            const std::string name = entry->d_name;
            if (name == "." || name == "..") continue;
            if (name == args[1]) {
                closedir(dir);
                std::cout << "True\n";
                return 0;
            }
            const auto child = path + "/" + name;
            struct stat info{};
            // Do not follow symlinks: a link back to an ancestor must not loop.
            if (lstat(child.c_str(), &info) == 0 && S_ISDIR(info.st_mode))
                pending.push_back(child);
        }
        closedir(dir);
    }
    std::cout << "False\n";
    return 1;
}
