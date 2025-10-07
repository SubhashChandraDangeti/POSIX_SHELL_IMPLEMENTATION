#include <iostream>
#include <string>
#include <vector>
#include <unistd.h>
#include <pwd.h>
#include <limits.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <signal.h>

#include "builtins/cd.h"
#include "builtins/pwd.h"
#include "builtins/echo.h"
#include "builtins/ls.h"
#include "utils/helpers.h"
#include "system/system.h"
#include "builtins/pinfo.h"
#include "builtins/search.h"
#include "builtins/history.h"


using namespace std;

string shellHomeDir;

int main() {
    // 1. Save shell's home directory
    setShellHome();

    // Load previous history from file
    loadHistory();
    signal(SIGCHLD, handle_sigchld); //tp avoid zombie processes
    // Shell loop
    while (true) {
        string username = getpwuid(geteuid())->pw_name;
        char hostname[HOST_NAME_MAX];
        gethostname(hostname, HOST_NAME_MAX);
        string cwd = getFormattedCWD();

        string prompt = "<" + username + "@" + hostname + ":" + cwd + "> ";
        char *input_cstr = readline(prompt.c_str());

        if (!input_cstr) break; // if user enter Ctrl+D then input_cstr return null
        string input_line(input_cstr);
        free(input_cstr);

        if (!input_line.empty()) {
            add_history(input_line.c_str());  // keep readline’s in-memory history (for arrow keys)
            addToHistory(input_line);         // update our persistent history (~/.my_shell_history)
        }


        vector<string> commands = splitCommand(input_line, ";");
        for (auto &cmd : commands) {
            trimSpaces(cmd);
            if (cmd.empty()) continue;

            vector<string> args = splitCommand(cmd);
            if (args.empty()) continue;
            if (args[0] == "pwd") runPWD();
            else if (args[0] == "echo") runEcho(args);
            else if (args[0] == "cd") runCD(args);
            else if(args[0]=="ls") runLS(args);
            else if (args[0] == "pinfo")  runPinfo(args);
            else if (args[0] == "search") runSearch(args);
            else if (args[0] == "history") runHistory(args); 
            else {
                runSystemCommand(args);
            }

        }
    }
    saveHistory();   // persist before exiting
    cout << "\nShell terminated." << endl;
    return 0;

}
