#include "ls.h"
#include <iostream>
#include <vector>
#include <string>
#include <dirent.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <iomanip>
#include <ctime>
#include <algorithm>

using namespace std;

// check if flag exists
bool hasFlag(const std::vector<std::string> &args, const std::string &flag) {
    for (auto &arg : args) {
        if (arg == flag) return true;
        if (arg.size() > 1 && arg[0] == '-') {
            // check for combined flags like -al
            if (flag == "-a" && arg.find('a') != std::string::npos) return true;
            if (flag == "-l" && arg.find('l') != std::string::npos) return true;
        }
    }
    return false;
}

// print permissions like rwxr-xr-x
void printPermissions(mode_t mode) {
    cout << ((S_ISDIR(mode)) ? "d" : "-");
    cout << ((mode & S_IRUSR) ? "r" : "-");
    cout << ((mode & S_IWUSR) ? "w" : "-");
    cout << ((mode & S_IXUSR) ? "x" : "-");
    cout << ((mode & S_IRGRP) ? "r" : "-");
    cout << ((mode & S_IWGRP) ? "w" : "-");
    cout << ((mode & S_IXGRP) ? "x" : "-");
    cout << ((mode & S_IROTH) ? "r" : "-");
    cout << ((mode & S_IWOTH) ? "w" : "-");
    cout << ((mode & S_IXOTH) ? "x" : "-");
}

// List one directory
void listDirectory(const string& path, bool showAll, bool longFormat) {

    DIR* dir = opendir(path.c_str());
    if (!dir) {
        perror("ls");
        return;
    }

    vector<string> files;
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        string name = entry->d_name;
        if (!showAll && name[0] == '.') continue; // skip hidden
        files.push_back(name);
    }
    closedir(dir);

    // sort alphabetically
    sort(files.begin(), files.end());

    // if -l flag: show detailed info
    if (longFormat) {
        for (auto &name : files) {
            string fullPath = path + "/" + name;
            struct stat st;
            if (stat(fullPath.c_str(), &st) == -1) {
                perror("stat");
                continue;
            }
            printPermissions(st.st_mode);
            cout << " " << setw(2) << st.st_nlink;

            struct passwd *pw = getpwuid(st.st_uid);
            struct group  *gr = getgrgid(st.st_gid);
            cout << " " << (pw ? pw->pw_name : to_string(st.st_uid));
            cout << " " << (gr ? gr->gr_name : to_string(st.st_gid));
            cout << " " << setw(6) << st.st_size;

            char timebuf[80];
            strftime(timebuf, sizeof(timebuf), "%b %d %H:%M", localtime(&st.st_mtime));
            cout << " " << timebuf;

            cout << " " << name << endl;
        }
        cout << flush;
    } else {
        // normal ls: just names
        for (auto &name : files) {
            cout << name << "  ";
        }
        cout << endl << flush;   
    }
}

// Main entry point for ls
void runLS(std::vector<std::string>& args) {
    bool showAll = hasFlag(args, "-a");
    bool longFormat = hasFlag(args, "-l");

    std::vector<std::string> targets;
    for (auto &arg : args) {
        if (arg[0] != '-') {
            if (arg != "ls") targets.push_back(arg);
        }
    }
    if (targets.empty()) targets.push_back("."); // default dir

    for (int i = 0; i < (int)targets.size(); i++) {
        if (targets.size() > 1) std::cout << targets[i] << ":" << std::endl;

        std::string path = targets[i];
        if (path == "~") {
            const char* home = getenv("HOME");
            if (home) path = std::string(home);
        }
        listDirectory(path, showAll, longFormat);
        if (i < (int)targets.size() - 1) std::cout << std::endl;
    }
}

