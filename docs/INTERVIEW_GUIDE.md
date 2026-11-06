# Understanding and presenting MyShell

Use this guide to prepare explanations, then verify each one in the source. The strongest interview demonstration is being able to predict behavior, explain the relevant system call, and reproduce an edge case.

## A short project explanation

“I built a Linux shell in C++ using POSIX process APIs. It parses commands into pipelines, connects stages through file descriptors, and uses process groups to manage foreground and background jobs. I implemented the required built-ins directly, plus redirection, persistent history, and completion. I tested both command output and actual terminal interactions, including Ctrl-C, Ctrl-Z, and nested shells.”

Call it an educational Unix shell. The parser supports a subset of shell syntax; it is not a full POSIX language interpreter or a Bash replacement.

## Questions to be ready for

### What happens when I type `cat < input | wc -l > count`?

1. The parser produces two commands, with redirects attached to the correct stages.
2. The executor creates a pipe and forks both children without waiting between them.
3. The children join one process group and wait for launch permission.
4. The parent hands that group the terminal and releases the launch gate.
5. `cat` reads from the input file and writes to the pipe. `wc` reads the pipe and writes to the output file.
6. Every process closes pipe descriptors it does not need. Once writers exit, `wc` sees EOF.
7. The parent waits for the job, reclaims the terminal, and returns the last stage's status.

Be able to sketch descriptors 0, 1, and the pipe endpoints on paper.

### Why do you need both `fork` and `exec`?

`fork` creates a separate process with inherited descriptor references. The child can modify its descriptors and signal behavior without modifying the shell's descriptor table. `execvp` replaces that child's program image with the requested executable. The PID stays the same across a successful exec. The parent remains the shell and waits or returns the prompt.

### Why does `cd` have to be a built-in?

The working directory belongs to a process. An external `cd` could change only its own directory. A standalone `cd` therefore calls `chdir` in the parent shell. In `cd nested | cat`, `cd` runs in a pipeline child, so the parent directory does not change.

### What is the difference between a file descriptor and an open file description?

A descriptor is a process-local integer referring to an underlying open file description. Descriptors inherited across `fork`, or created with `dup`, can refer to the same description and share its file offset. `dup2(source, target)` replaces the target descriptor so standard input/output can point at a file or pipe.

### Why can a pipeline deadlock?

If the parent waits for the producer before starting the consumer, the producer can fill the pipe buffer and block. It cannot finish until someone reads. Another hang occurs if any process retains a pipe write end: readers cannot see EOF even after the intended writer exits. This shell starts all stages before waiting and closes unused ends in both parent and children.

Demonstrate with `seq 1 200000 | cat | wc -l`, then `yes x | head -n 1`. The second case also checks that SIGPIPE terminates an upstream producer when the consumer exits early.

### How do Ctrl-C and Ctrl-Z affect all stages?

All pipeline stages share a process group. `tcsetpgrp` makes that group the terminal foreground group. Terminal-generated SIGINT and SIGTSTP go to that entire group. The shell retains its own group and regains terminal ownership after the job exits or stops.

Ctrl-Z **stops** execution. `bg` sends SIGCONT to resume it while the shell keeps the terminal. A background job that tries to read the terminal normally gets SIGTTIN and stops; `fg` can give it the terminal again.

### Why is the launch gate useful?

Without it, an early child could run or exit before all stages join its process group. An interactive program could also try to read the terminal before the parent hands it over. The gate is a pipe: children block reading it; after group construction and handoff, the parent closes the write end. Every child sees EOF and continues.

### What is a zombie, and how do you avoid it?

A terminated child retains a process-table status entry until its parent waits for it. The foreground waiter collects statuses and routes them by PID to the correct job, including background children that finish during a long foreground command. Background jobs are also polled with nonblocking `waitpid` between commands and while Readline is waiting. Completed jobs are removed from the tracker when notifications are printed. There can be a short scheduling delay between a child exiting and the shell collecting its status.

### Why not print from a SIGCHLD handler?

C++ streams and allocation are not async-signal-safe. A signal can interrupt those same facilities, leaving internal state inconsistent if the handler reenters them. The handler only sets a `sig_atomic_t` flag; normal code performs waiting, state updates, and notifications.

`waitpid` itself can be used in carefully designed signal handlers, but putting all reaping in this shell's normal control flow keeps foreground/background ownership easier to reason about.

### How do redirects work on a parent built-in?

Save stdin/stdout with `dup`, apply file redirects with `open` and `dup2`, run the built-in, then restore the saved descriptors. The restoration is scoped through a destructor so early returns and exceptions do not leave the prompt redirected. Flush C++ output before switching descriptors and before restoring them.

### Why `lstat` instead of `stat`?

`stat` follows a symbolic link; `lstat` describes the link itself. `ls -l` needs link metadata and its target text. Recursive search uses `lstat` so a link to a parent directory does not create an endless traversal.

### What makes `/proc/<pid>/stat` parsing tricky?

The executable's command name is enclosed in parentheses and can contain spaces or parentheses. Splitting the entire line on spaces shifts the numeric fields incorrectly. This implementation finds the final closing parenthesis and then extracts the state, group, terminal, and terminal foreground group. Matching group IDs with a controlling terminal determines the `+` marker.

### How does history persist?

A deque retains the latest twenty distinct consecutive entries. Readline gets those entries at startup, so arrow keys work across sessions. On each new entry, the shell writes a complete temporary file and renames it over the history file. This avoids exposing a partially written destination. Concurrent sessions can overwrite each other's histories; there is no merge or file-locking protocol.

### What does completion do, and what does Readline provide?

The project supplies candidate generation for built-ins, executable names found in `PATH`, and files/directories. Readline handles editing, key bindings, common-prefix insertion, and displaying ambiguous candidates. Be clear that line editing was integrated from a library, not implemented from scratch.

## A five-minute demonstration

Start in the project directory with a temporary history path:

```sh
MYSHELL_HISTORY=/tmp/myshell-interview-history ./myshell
```

Then demonstrate:

```sh
pwd; cd files; pwd; cd -
echo "hello shell" | tr a-z A-Z > /tmp/myshell-result
cat < /tmp/myshell-result
seq 1 200000 | wc -l
search search.txt
pinfo
sleep 30 | cat
```

Press Ctrl-Z, run `jobs`, run `bg`, then `fg`, and press Ctrl-C. Show a unique Tab completion such as `pinf<Tab>`, then `history 5`. Exit with Ctrl-D, restart with the same history path, and use the Up arrow. End by running `make test` and `make test-interactive` outside MyShell.

## Resume wording

**MyShell — Linux Shell in C++ | POSIX, Process Control, IPC**

- Built a modular C++17 shell supporting concurrent pipelines, I/O redirection, and foreground/background execution using `fork`, `execvp`, `pipe`, and `dup2`.
- Implemented process-group job control, terminal handoff, signal handling, required Unix-style built-ins, persistent history, and Readline completion.
- Added isolated integration tests and pseudo-terminal tests covering pipeline behavior, job resumption, terminal signals, completion, and history across sessions.

Use wording you can explain and demonstrate yourself. Add a repository link after you publish it. Avoid unsupported performance numbers, claims of production readiness, or claims of full POSIX compliance.

## Tradeoffs and next steps to discuss

The implementation chooses explicit process/job records and a single-threaded loop so status ownership is visible. Rolling pipe construction keeps parent descriptor use small as pipeline length grows. It still needs one process per stage and is bounded by operating-system limits.

Potential extensions include concurrent-session history merging, richer job notifications with exit causes, an explicit self-pipe event loop, more complete quoting rules, variable expansion, globbing, and `&&`/`||`. Each should come with an explicit grammar or lifecycle rule and behavioral tests.
