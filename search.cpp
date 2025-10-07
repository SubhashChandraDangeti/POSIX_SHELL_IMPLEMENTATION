#include "search.h"
#include <bits/stdc++.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <limits.h>
using namespace std;

void runSearch(vector<string>& args) {
    if (args.size() != 2) {
        cout << "Invalid arguments.Please enter filename to search for!";
        return;
    }

    string target = args[1];

    // Get current working directory
    char cwdbuf[PATH_MAX];
    string cwd;
    if (getcwd(cwdbuf, sizeof(cwdbuf)) != nullptr) cwd = cwdbuf;
    else cwd = "."; // fallback

    // Stack for DFS traversal
    stack<string> st;
    st.push(cwd);

    bool found = false;

    while (!st.empty()) {
    string dirpath = st.top();
    st.pop();

    DIR *dp = opendir(dirpath.c_str());
    if (!dp) continue;

    bool dirFound = false;  

    struct dirent *entry;
    while ((entry = readdir(dp)) != nullptr) {
        string name = entry->d_name;
        if (name == "." || name == "..") continue;

        if (name == target) {
            found = true;
            dirFound = true; 
            break;            // stop scanning this dir
        }

        string fullpath = dirpath + "/" + name;
        struct stat stbuf;
        if (lstat(fullpath.c_str(), &stbuf) == -1) continue;

        if (S_ISDIR(stbuf.st_mode)) {
            st.push(fullpath);
        }
    }

    closedir(dp);  

    if (dirFound) break;  // exit outer loop cleanly
}


    if (found) cout << "True\n";
    else cout << "False\n";
}
