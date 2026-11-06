#include "ls.h"
#include "../utils/helpers.h"
#include <algorithm>
#include <cstdio>
#include <ctime>
#include <dirent.h>
#include <grp.h>
#include <iomanip>
#include <iostream>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>

namespace {
struct Entry {
    std::string name, path, owner, group, size;
    struct stat info{};
};

std::string permissions(mode_t mode) {
    std::string result = "----------";
    if (S_ISDIR(mode)) result[0] = 'd';
    else if (S_ISLNK(mode)) result[0] = 'l';
    else if (S_ISCHR(mode)) result[0] = 'c';
    else if (S_ISBLK(mode)) result[0] = 'b';
    else if (S_ISFIFO(mode)) result[0] = 'p';
    else if (S_ISSOCK(mode)) result[0] = 's';
    const mode_t bits[] = {S_IRUSR, S_IWUSR, S_IXUSR, S_IRGRP, S_IWGRP,
                          S_IXGRP, S_IROTH, S_IWOTH, S_IXOTH};
    for (int i = 0; i < 9; ++i)
        if (mode & bits[i]) result[i + 1] = "rwx"[i % 3];
    if (mode & S_ISUID) result[3] = mode & S_IXUSR ? 's' : 'S';
    if (mode & S_ISGID) result[6] = mode & S_IXGRP ? 's' : 'S';
    if (mode & S_ISVTX) result[9] = mode & S_IXOTH ? 't' : 'T';
    return result;
}

bool readEntry(const std::string& path, const std::string& name, Entry& entry) {
    entry.path = path;
    entry.name = name;
    if (lstat(path.c_str(), &entry.info) < 0) {
        perror(("ls: " + path).c_str());
        return false;
    }
    const auto* user = getpwuid(entry.info.st_uid);
    entry.owner = user ? user->pw_name : std::to_string(entry.info.st_uid);
    const auto* group = getgrgid(entry.info.st_gid);
    entry.group = group ? group->gr_name : std::to_string(entry.info.st_gid);
    if (S_ISCHR(entry.info.st_mode) || S_ISBLK(entry.info.st_mode))
        entry.size = std::to_string(major(entry.info.st_rdev)) + ", " +
                     std::to_string(minor(entry.info.st_rdev));
    else entry.size = std::to_string(entry.info.st_size);
    return true;
}

void printEntries(const std::vector<Entry>& entries, bool detailed, bool directory) {
    std::size_t links = 1, owner = 1, group = 1, size = 1;
    long long blocks = 0;
    for (const auto& entry : entries) {
        links = std::max(links, std::to_string(entry.info.st_nlink).size());
        owner = std::max(owner, entry.owner.size());
        group = std::max(group, entry.group.size());
        size = std::max(size, entry.size.size());
        blocks += entry.info.st_blocks;
    }
    if (detailed && directory) std::cout << "total " << blocks / 2 << '\n';
    for (const auto& entry : entries) {
        if (detailed) {
            const auto& info = entry.info;
            char timestamp[80] = {};
            const auto now = std::time(nullptr);
            const bool recent = info.st_mtime <= now && now - info.st_mtime < 15778476;
            if (const auto* time = std::localtime(&info.st_mtime))
                std::strftime(timestamp, sizeof(timestamp), recent ? "%b %e %H:%M" : "%b %e  %Y", time);
            std::cout << permissions(info.st_mode) << ' ' << std::right << std::setw(links)
                      << info.st_nlink << ' ' << std::left << std::setw(owner) << entry.owner
                      << ' ' << std::setw(group) << entry.group << ' ' << std::right
                      << std::setw(size) << entry.size << ' ' << timestamp << ' ';
        }
        std::cout << entry.name;
        if (detailed && S_ISLNK(entry.info.st_mode)) {
            std::vector<char> link(256);
            for (;;) {
                const auto length = readlink(entry.path.c_str(), link.data(), link.size());
                if (length < 0) { perror("ls: readlink"); break; }
                if (static_cast<std::size_t>(length) < link.size()) {
                    std::cout << " -> " << std::string(link.data(), length);
                    break;
                }
                link.resize(link.size() * 2);
            }
        }
        std::cout << '\n';
    }
}
}

int runLS(const std::vector<std::string>& args) {
    bool all = false, detailed = false, options = true;
    std::vector<std::string> targets;
    for (std::size_t i = 1; i < args.size(); ++i) {
        const auto& arg = args[i];
        if (options && arg == "--") { options = false; continue; }
        if (options && arg.size() > 1 && arg[0] == '-') {
            for (std::size_t j = 1; j < arg.size(); ++j) {
                if (arg[j] == 'a') all = true;
                else if (arg[j] == 'l') detailed = true;
                else { std::cerr << "ls: unsupported option: " << arg[j] << '\n'; return 1; }
            }
        } else targets.push_back(arg);
    }
    if (targets.empty()) targets.push_back(".");
    int status = 0;
    for (std::size_t i = 0; i < targets.size(); ++i) {
        const auto& path = targets[i];
        Entry target;
        if (!readEntry(path, targets[i], target)) { status = 1; continue; }
        bool directory = S_ISDIR(target.info.st_mode);
        if (!detailed && S_ISLNK(target.info.st_mode)) {
            struct stat followed{};
            directory = stat(path.c_str(), &followed) == 0 && S_ISDIR(followed.st_mode);
        }
        if (!directory) { printEntries({target}, detailed, false); continue; }
        DIR* dir = opendir(path.c_str());
        if (!dir) { perror(("ls: " + path).c_str()); status = 1; continue; }
        if (targets.size() > 1) {
            if (i) std::cout << '\n';
            std::cout << targets[i] << ":\n";
        }
        std::vector<Entry> entries;
        while (auto* item = readdir(dir)) {
            const std::string name = item->d_name;
            if (!all && name[0] == '.') continue;
            Entry entry;
            if (readEntry(path + "/" + name, name, entry)) entries.push_back(std::move(entry));
            else status = 1;
        }
        closedir(dir);
        std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
            return a.name < b.name;
        });
        printEntries(entries, detailed, true);
    }
    return status;
}
