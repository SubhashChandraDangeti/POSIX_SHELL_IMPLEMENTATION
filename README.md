# MyShell

**A modular Linux shell built in C++17 using POSIX process and I/O APIs.**

Run commands, connect pipelines, redirect input and output, and manage foreground and background jobs—all from a custom interactive prompt.

[Quick Start](#quick-start) · [Features](#features) · [Usage](#usage) · [Testing](#testing) · [Design Guide](docs/DESIGN.md)

```text
<username@hostname:~> echo "hello shell" | tr a-z A-Z
HELLO SHELL

<username@hostname:~> sleep 10 &
[1] 12345

<username@hostname:~> jobs
[1] Running 12345 sleep 10
```

## Features

| Feature | Support |
| --- | --- |
| Command execution | External programs, arguments, semicolon-separated commands, foreground and background execution |
| Pipelines and redirection | Concurrent pipe stages, `<`, `>`, `>>`, and redirection within pipelines |
| Job control | Process groups, terminal handoff, `jobs`, `fg`, and `bg` |
| Built-in commands | `cd`, `echo`, `pwd`, `ls`, `pinfo`, `search`, `history`, and `exit` |
| Interactive editing | Tab completion, Up/Down history navigation, Ctrl-C, Ctrl-Z, and Ctrl-D |
| Persistent history | Latest 20 commands across sessions; `history` displays 10 by default |
| Input handling | Spaces and tabs, basic quotes, empty arguments, and launch-directory tilde expansion |

## Quick Start

### Prerequisites

- Linux
- A C++17 compiler and Make
- GNU Readline development headers

On Debian or Ubuntu:

```sh
sudo apt install g++ make libreadline-dev
```

### Build and launch

From the repository directory:

```sh
make
./myshell
```

The prompt discovers your username and hostname automatically. The **directory from which you launch MyShell** becomes its home, displayed as `~`. This stays fixed after `cd` and is separate from your account's `$HOME`.

To remove the executable and generated build files:

```sh
make clean
```

## Usage

### Built-ins and navigation

```sh
pwd
ls -al .
cd files; pwd; cd ~
cd -
search search.txt
pinfo
history 5
```

`ls` supports `-a`, `-l`, combined flags, and multiple file or directory operands. `pinfo [pid]` displays process state, virtual memory, and executable path. `search name` recursively looks for a file or directory below the current directory.

### Pipes and redirection

```sh
echo first > /tmp/myshell-demo.txt
echo second >> /tmp/myshell-demo.txt
cat < /tmp/myshell-demo.txt | tr a-z A-Z | head -n 2
```

Pipe stages run concurrently. Redirects apply in written order, and new output files use mode `0644`, subject to the current umask.

### Background jobs

```sh
sleep 10 &
jobs
fg
```

A background launch prints `[job-id] process-group-id`; the group ID is also the first child's PID. Use `fg [id]` to bring a job to the foreground or `bg [id]` to resume a stopped job in the background. Both accept a number or `%number` and default to the most recently registered remaining job.

| Key | Action |
| --- | --- |
| Ctrl-C | Interrupt the foreground job; clear input when at the prompt |
| Ctrl-Z | Stop the foreground job so it can be resumed with `fg` or `bg` |
| Ctrl-D | Exit at an empty prompt |
| Tab | Complete commands, files, and directories |
| Up / Down | Navigate command history, including saved entries from previous sessions |

Try `sleep 30 | cat`, press Ctrl-Z, run `jobs`, then `bg`, then `fg`, and press Ctrl-C.

### Batch input

You can also pass a command stream without interactive prompts:

```sh
printf 'echo hello | tr a-z A-Z\npwd\nexit\n' | ./myshell
```

## Project Structure

```text
.
├── Main.cpp                  # Input loop and prompt
├── builtins/                 # Built-in commands and command registry
├── system/                   # Process execution, pipelines, and job control
├── utils/                    # Parsing, completion, and path helpers
├── tests/                    # Parser, integration, and terminal tests
├── docs/
│   └── DESIGN.md             # Architecture and resource ownership
├── files/                    # Sample files for manual practice
├── commands.txt              # Manual practice checklist
├── Makefile
└── README.md
```

Generated objects and dependency files live in `build/` and are excluded by `.gitignore`.

## Testing

```sh
make test
make test-interactive
```

- **Parser tests:** quoted arguments, operators, tilde expansion, redirection order, and invalid syntax.
- **Integration tests:** large pipelines, early consumer exit, stream restoration, built-in state, background cleanup, and history across sessions.
- **Terminal tests:** real pseudo-terminal input, signals, job resumption, nested shells, completion, and arrow navigation.

All tests are written in C++ and use isolated temporary directories and history files. Integration tests require standard coreutils programs; terminal tests also require Linux `libutil` and an environment that permits pseudo-terminals.

## Documentation

| Guide | What you will find |
| --- | --- |
| [Design walkthrough](docs/DESIGN.md) | Parsing, descriptor ownership, process groups, terminal handoff, and error handling |
| [Practice checklist](commands.txt) | Commands and interactive behaviors to try yourself |

<details>
<summary><strong>Syntax and execution details</strong></summary>

- `;` separates commands; `&` starts the preceding pipeline in the background. Operators work without surrounding spaces.
- Single and double quotes preserve grouped arguments, whitespace, and operators. Empty quotes preserve an empty argument. Backslash escapes one character outside quotes.
- Unquoted `~` and `~/` expand to the launch directory in arguments and redirect filenames. Quoted or escaped tildes remain literal.
- Syntax errors reject the entire input line before any command on that line runs.
- Standalone foreground built-ins run in the parent shell. Their redirects are temporary, so `cd directory > file` still changes the shell's directory and restores stdout afterward.
- Built-ins inside pipelines or background commands run in children; their state changes do not affect the parent. `jobs`, `fg`, and `bg` require standalone foreground use.
- Redirects apply after pipe connections, in written order. `echo hello > first > second` creates or truncates both files and writes into `second`.
- A foreground pipeline returns its last stage's status. Missing executables return 127; other execution failures return 126. Syntax errors set status 2; Ctrl-C at the prompt sets status 130. `exit [status]` accepts an explicit status; otherwise, exit and EOF use the last command status.
- Completed background jobs report `Done`; stopped jobs remain registered. On exit, the shell sends remaining groups SIGHUP followed by SIGCONT and restores terminal ownership and modes.

</details>

<details>
<summary><strong>History and completion settings</strong></summary>

History defaults to `$HOME/.my_shell_history`. To choose another file:

```sh
MYSHELL_HISTORY=/tmp/myshell-practice-history ./myshell
```

Blank lines and consecutive duplicates are omitted. The current command enters history before execution. Each accepted entry is saved using a temporary file and rename, with mode `0600`. Concurrent sessions use last-writer-wins persistence; their histories are not merged.

Tab completes a unique match, extends a common prefix, or displays multiple candidates through Readline. Candidates include built-ins, executable names in `PATH`, and files/directories. Results depend on your environment: `ec<Tab>` can be ambiguous if another executable begins with `ec`.

</details>

## Scope and Limitations

MyShell supports interactive command execution with a focused subset of shell syntax. Variable and command substitution, globbing, `&&`/`||`, multiline input, user-specific tilde expansion, descriptor-specific redirects such as `2>`, and `pipefail` are outside its scope.

Pipeline size is bounded by operating-system resources. `ls` supports `-a` and `-l` but does not reproduce every GNU option, locale rule, or ACL/security-context marker. `pinfo` relies on Linux `/proc`; inaccessible or disappearing process data is reported as unavailable.
