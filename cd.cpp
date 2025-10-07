#include <iostream>
#include <unistd.h>
#include <limits.h>
#include "cd.h"

using namespace std;

extern string shellHomeDir;
string prevDir = "";

void runCD(vector<string> &args) {
    if(args.size()>2) {
        cout << "Invalid arguments" << endl;
        return;
    }
    string target;
    char cwd[PATH_MAX];
    getcwd(cwd, sizeof(cwd));
    string currDir = cwd;

    if(args.size()==1 || args[1]=="~") {
        target = shellHomeDir;
    } else if(args[1]=="-") {
        if(prevDir.empty()) {
            cerr << "cd: oldpwd not set" << endl;
            return;
        }
        target = prevDir;
        cout << target << endl;
    } else {
        target = args[1];
    }

    if(chdir(target.c_str()) != 0) {
        perror("cd");
        return;
    }
    prevDir = currDir;
}
