# Advanced Operating Systems – Assignment 2
**POSIX Shell Implementation**  
Roll Number: *2025201007*  

---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
## Directory Structure  
The project is organized in a modular way, with separate directories for built-in commands, system command handling, and utility/helper functions.  

2025201007_A2
├── builtins/        # cd, pwd, echo, ls, pinfo, search, history
├── system/          # handles external system commands
├── utils/           # helper functions (prompt, parsing, formatting, etc.)
├── Main.cpp         # entry point of the shell
├── Makefile         # build instructions
└── myshell          # compiled executable once u execute will get it

Each built-in command has its own `*.cpp` and `*.h` file. Object files (`*.o`) are generated during compilation by the Makefile.  
---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
## Build Instructions 
1. To clean object files and the executable:  
```bash
make clean
```   
2. To compile the project:  
```bash
make
```  
3. ## Run Instructions  
After building with `make`, start the shell using:  
```bash
./myshell
```  
This will launch the custom shell with the prompt:  
```
<username@hostname:cwd>
```  
---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
# Tasks Implemented  

# Task 1: Prompt Display  

## 1) Approach  
1) At startup, save the shell’s home directory.  
2) For each prompt:  
   - Fetch **username** (`getpwuid(geteuid())`).  
   - Fetch **hostname** (`gethostname()`).  
   - Get **current working directory** (`getcwd()`) and format it (replace home path with `~`).  
3) Construct prompt:  
   ```
   <username@hostname:cwd>
   ```  
4) Use `readline()` for input with editing and history support.  

## 2) System / Utility Calls  
- `getcwd()` → get current directory.  
- `getpwuid(geteuid())` → fetch username.  
- `gethostname()` → fetch system name.  
- `readline()` / `add_history()` → interactive input & arrow key navigation.  
- `signal(SIGCHLD, handler)` → reap background processes.  
---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
# Task 2: Built-in Commands – `pwd`, `echo`, `cd`  

## pwd  
### 1) Approach  
1) Calls helper function `getFormattedCWD()` (uses `getcwd()`).  
2) Prints the directory, replacing the home directory path with `~`.  

### 2) System / Utility Calls  
- `getcwd()` → fetch current directory.  
- `getFormattedCWD()` (utility) → formats the directory path.  
---
## echo  
### 1) Approach  
1) Iterates over all arguments after `echo`.  
2) Prints them separated by spaces, ending with newline.  
3) Handles multiple spaces/tabs since arguments are tokenized beforehand.  

### 2) System / Utility Calls  
- Uses `cout` for output.  
- No direct system calls.  
---
## cd  
### 1) Approach  
1) If no arg or `~` → go to shell’s home directory.  
2) If `-` → switch to previous directory (`prevDir`).  
3) If a path is given → change to that path.  
4) If more than one arg → print error.  
5) Update `prevDir` after successful change.  

### 2) System / Utility Calls  
- `getcwd()` → get current directory.  
- `chdir(target)` → change directory.  
- `perror("cd")` → print error on failure.  
---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
# Task 3: `ls` Implementation  
## 1) Approach  
1) Parse flags:  
   - `-a` → show hidden files.  
   - `-l` → show detailed info.  
2) Collect targets (default: `.`).  
3) For each directory:  
   - Open with `opendir()`.  
   - Iterate entries with `readdir()`.  
   - Skip hidden files unless `-a`.  
   - Sort alphabetically.  
4) If `-l`:  
   - Use `stat()` for metadata.  
   - Print permissions, owner, group, size, and last modified time.  
5) Else: print file names in a row.  

## 2) System / Utility Calls  
- `opendir()`, `readdir()`, `closedir()` → read directories.  
- `stat()` → get file info.  
- `getpwuid()`, `getgrgid()` → owner & group names.  
- `strftime()` + `localtime()` → format time.  
- `getenv("HOME")` → expand `~`.  
---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
# Task 4: System Commands (Foreground & Background Execution)  
## 1) Approach  
1) Check if command ends with `&`.  
   - If yes → run as **background**.  
   - Else → run as **foreground**.  
2) Convert arguments to `char*[]` for `execvp()`.  
3) `fork()` a child:  
   - **Child** → execute with `execvp()`.  
   - **Parent** →  
     - Foreground → `waitpid()` until child exits.  
     - Background → print PID and continue.  
4) Handle terminated background jobs using `SIGCHLD` handler (`waitpid(..., WNOHANG)`).  

## 2) System / Utility Calls  
- `fork()` → create child process.  
- `execvp()` → run system command.  
- `waitpid()` → wait for child (foreground).  
- `waitpid(..., WNOHANG)` → reap background jobs.  
- `signal(SIGCHLD, handler)` → register handler.  
- `perror()` → error handling.  
---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
# Task 5: `pinfo` Implementation  
## 1) Approach  
1) If no PID → use shell’s PID.  
2) Read `/proc/<pid>/status` → extract process state.  
3) Read `/proc/<pid>/statm` → extract memory usage.  
4) Use `readlink("/proc/<pid>/exe")` → get executable path.  
5) Print status, memory, and path.  

## 2) System / Utility Calls  
- `getpid()` → get shell PID.  
- `ifstream` → read `/proc/<pid>/status` and `/proc/<pid>/statm`.  
- `readlink()` → read executable path.  
- `perror()` → error handling.  
---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
# Task 6: `search` Implementation  
## 1) Approach  
1) Requires exactly one argument (`search <name>`).  
2) Get current directory (`getcwd()`).  
3) Perform DFS using a stack:  
   - Open directory with `opendir()`.  
   - For each entry → check match.  
   - If subdirectory → push to stack.  
4) Print `True` if found, else `False`.  

## 2) System / Utility Calls  
- `getcwd()` → starting point.  
- `opendir()`, `readdir()`, `closedir()` → traverse directories.  
- `lstat()` → check if entry is a directory.  
---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
# Task 12: `history` Implementation  
## 1) Approach  
1) Maintain persistent history in `~/.my_shell_history`.  
2) Use `deque` to store up to 20 commands in memory.  
3) On new command → add to history (avoid consecutive duplicates).  
4) On `history`:  
   - Print last 10 commands by default.  
   - Or last `n` commands if `history n`.  

## 2) System / Utility Calls  
- `open(..., O_RDONLY)` + `read()` + `close()` → load history.  
- `open(..., O_WRONLY|O_CREAT|O_TRUNC)` + `write()` → save history.  
- `deque<string>` → maintain fixed-size list.  
- `stringstream` + `getline()` → parse file contents.  
- `stoi()` → convert numeric args.  
---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
