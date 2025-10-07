#include <iostream>
#include "echo.h"

using namespace std;

void runEcho(const vector<string>& args) {
    for (size_t i = 1; i < args.size(); i++) {
        cout << args[i];
        if (i < args.size() - 1) cout << " ";
    }
    cout << endl;
}
