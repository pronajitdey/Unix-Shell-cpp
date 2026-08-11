#include "shell/executor.h"
#include "shell/path_utils.h"
#include "shell/completion_registry.h"
#include "shell/job_manager.h"
#include "shell/history.h"
#include "shell/variables.h"

#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include <sys/wait.h>
#include <fstream>
#include <fcntl.h>
#include <unordered_map>
#include <iomanip>
#include <readline/history.h>

static bool isBuiltinName(const std::string& name) {
  return name == "echo" || name == "pwd" || name == "cd" ||
         name == "type" || name == "complete" || name == "jobs" ||
         name == "history" || name == "declare";
}

// Shared helper: given the current (already-reaped) job list, print one
// line per job in `jobsToShow`, using markers computed relative to the
// FULL current table (`allJobsSnapshot`), not just the subset being shown.
static char markerFor(size_t indexInFullList, size_t fullListSize) {
  if (indexInFullList == fullListSize - 1) return '+';
  if (fullListSize >= 2 && indexInFullList == fullListSize - 2) return '-';
  return ' ';
}

static void printJobLine(const Job& job, char marker) {
  std::string status = job.running ? "Running" : "Done";
  std::string displayLine = job.commandLine;
  if (job.running) {
    displayLine += " &";
  }

  std::cout << "[" << job.number << "]" << marker << "  "
            << std::left << std::setw(24) << status
            << displayLine << std::endl;
}

// Helper to reconstruct a readable command line for job registration
// (not used for execution — just for future `jobs` listing/display).
// static std::string reconstructCommandLine(const Command& cmd) {
//     std::string line = cmd.name;
//     for (const auto& arg : cmd.args) {
//         line += " " + arg;
//     }
//     return line;
// }

// Used automatically before each prompt: reap, announce ONLY the newly
// finished jobs (markers computed over the full set at reap time, since
// that's still the current table at the moment they're shown), then
// remove them.
void reapAndAnnounceFinishedJobs() {
  reapFinishedJobs(); // mark exited jobs

  const auto& jobs = allJobs();
  size_t n = jobs.size();

  for (size_t i = 0; i < n; ++i) {
    if (!jobs[i].running) {
      printJobLine(jobs[i], markerFor(i, n));
    }
  }

  removeFinishedJobs();
}

// handle echo command
static void runEcho(const Command& cmd) {
  for (size_t i = 0; i < cmd.args.size(); i++) {
    std::cout << cmd.args[i];
    if (i + 1 < cmd.args.size()) std::cout << " ";
  }
  std::cout << std::endl;
}

// handle type command
static void runType(const Command& cmd) {
  if (cmd.args.empty()) return;
  const std::string& target = cmd.args[0];

  if (target == "echo" 
      || target == "exit" 
      || target == "type" 
      || target == "pwd"
      || target == "cd"
      || target == "complete"
      || target == "jobs"
      || target == "history"
      || target == "declare") {    // check builtin commands
    std::cout << target << " is a shell builtin" << std::endl;
    return;
  }

  const char* env_path = std::getenv("PATH");
  fs::path found = env_path ? findExecutableFilePath(env_path, target) : fs::path{};

  if (!found.empty()) {
    std::cout << target << " is " << found.string() << std::endl;
  } else {
    std::cout << target << ": not found" << std::endl;
  }
}

// execute external commands
// static void runExternal(const Command& cmd) {
//   const char* env_path = std::getenv("PATH");
//   fs::path command_path = env_path ? findExecutableFilePath(env_path, cmd.name) : fs::path{};

//   if (command_path.empty()) {
//     std::cout << cmd.name << ": command not found" << std::endl;
//     return;
//   }

//   pid_t pid = fork();

//   if (pid < 0) {
//     perror("fork");
//     return;
//   }

//   if (pid == 0) {
//     if (!cmd.stdout_redirect.empty()) {
//       int flags = O_WRONLY | O_CREAT | (cmd.stdout_append ? O_APPEND : O_TRUNC);
//       int fd = open(cmd.stdout_redirect.c_str(), flags, 0644);

//       if (fd < 0) {
//         perror("open");
//         _exit(1);
//       }
//       // only child's file descriptor is changed
//       dup2(fd, STDOUT_FILENO);
//       close(fd);
//     }

//     if (!cmd.stderr_redirect.empty()) {
//       int flags = O_WRONLY | O_CREAT | (cmd.stderr_append ? O_APPEND : O_TRUNC);
//       int fd = open(cmd.stderr_redirect.c_str(), flags, 0644);

//       if (fd < 0) { 
//         perror("open"); 
//         _exit(1); 
//       }

//       dup2(fd, STDERR_FILENO);
//       close(fd);
//     }

//     std::vector<char*> argv;
//     argv.push_back(const_cast<char*>(cmd.name.c_str()));
//     for (auto& a : cmd.args) argv.push_back(const_cast<char*>(a.c_str()));
//     argv.push_back(nullptr);

//     // new program inherits the child's file descriptors
//     execvp(cmd.name.c_str(), argv.data());

//     // only reached if execvp fails
//     perror("execvp");
//     _exit(127);   // 127 for command not found (in shell scripts)
//   }

//   // Parent process
//   if (cmd.background) {
//     int jobNumber = registerBackgroundJob(pid, reconstructCommandLine(cmd));
//     std::cout << "[" << jobNumber << "] " << pid << std::endl;
//     // Deliberately no waitpid() here — that's what makes it non-blocking.
//   } else {
//     int status;
//     waitpid(pid, &status, 0);
//   }
// }

// run pwd builtin command
static void runPwd() {
  std::cout << fs::current_path().string() << std::endl;
}

static void runCd(const Command& cmd) {
  if (cmd.args.empty()) {
    return;   // actually should go to HOME: TODO
  }

  std::string target = cmd.args[0];

  if (target == "~") {
    const char* home_env = getenv("HOME");
    if (home_env) target = home_env;
  }

  if (!fs::exists(target) || !fs::is_directory(target)) {
    std::cout << "cd: " << target << ": No such file or directory" << std::endl;
    return;
  }

  try {
    fs::current_path(target);
  } catch (const fs::filesystem_error&) {
    std::cout << "cd: " << target << ": No such file or directory" << std::endl;
  }
}

static void runComplete(const Command& cmd) {
  if (cmd.args.size() >= 2 && cmd.args[0] == "-p") {
    const std::string& target = cmd.args[1];
    auto& registry = completionRegistry();
    auto it = registry.find(target);

    if (it == registry.end()) {
      std::cout << "complete: " << target << ": no completion specification" << std::endl;
    } else {
      std::cout << "complete -C '" << it->second << "' " << target << std::endl;
    }
    return;
  }

  if (cmd.args.size() >= 3 && cmd.args[0] == "-C") {
    const std::string& script_path = cmd.args[1];
    const std::string& command_name = cmd.args[2];
    completionRegistry()[command_name] = script_path;
    return;
  }

  if (cmd.args.size() >= 2 && cmd.args[0] == "-r") {
    const std::string& target = cmd.args[1];
    completionRegistry().erase(target);
    return;
  }
}

// Used by the `jobs` builtin: reap (mark only), display EVERYTHING
// together in job-number order with markers over the full set, then
// remove Done entries.
static void runJobs(const Command& cmd) {
  (void)cmd;

  reapFinishedJobs(); // mark only — no display, no removal yet

  const auto& jobs = allJobs();
  size_t n = jobs.size();

  for (size_t i = 0; i < n; i++) {
    printJobLine(jobs[i], markerFor(i, n));
  }

  removeFinishedJobs();
}

// A valid identifier: starts with a letter or underscore, followed by
// any number of letters, digits, or underscores.
static bool isValidIdentifier(const std::string& name) {
  if (name.empty()) return false;

  char first = name[0];
  if (!std::isalpha(static_cast<unsigned char>(first)) && first != '_') {
    return false;
  }

  for (size_t i = 1; i < name.size(); ++i) {
    char c = name[i];
    if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') {
      return false;
    }
  }

  return true;
}

static void runHistory(const Command& cmd) {
  // read history from a file
  if (!cmd.args.empty() && cmd.args[0] == "-r") {
    if (cmd.args.size() < 2) return;  // no path given

    loadHistoryFromFile(cmd.args[1]);

    return;
  }

  // write history to a file
  if (!cmd.args.empty() && cmd.args[0] == "-w") {
    if (cmd.args.size() < 2) return;  // no path given

    saveHistoryToFile(cmd.args[1]);

    return;
  }

  // append history to a file since last write or append
  if (!cmd.args.empty() && cmd.args[0] == "-a") {
    if (cmd.args.size() < 2) return;  // no path given

    appendHistoryToFile(cmd.args[1]);

    return;
  }

  const auto& history = getHistory();

  size_t startIndex = 0;

  if (!cmd.args.empty()) {
    try {
      int n = std::stoi(cmd.args[0]);
      if (n < 0) n = 0;

      size_t count = static_cast<size_t>(n);
      startIndex = (count >= history.size()) ? 0 : history.size() - count;
    } catch (const std::exception&) {
      // Non-numeric argument - fall back to showing everything
      startIndex = 0;
    }
  }

  for (size_t i = startIndex; i < history.size(); i++) {
    std::cout << std::setw(5) << (i + 1) << " " << history[i] << std::endl;
  }
}

static void runDeclare(const Command& cmd) {
  if (cmd.args.size() >= 2 && cmd.args[0] == "-p") {
    const std::string& target = cmd.args[1];
    std::string value;

    if (getVariable(target, value)) {
      std::cout << "declare -- " << target << "=\"" << value << "\"" << std::endl;
    } else {
      std::cout << "declare: " << target << ": not found" << std::endl;
    }
    return;
  }

  // declare NAME=VALUE
  if (!cmd.args.empty()) {
    const std::string& assignment = cmd.args[0];
    size_t eq = assignment.find('=');

    if (eq != std::string::npos) {
      std::string name = assignment.substr(0, eq);
      std::string value = assignment.substr(eq + 1);

      if (!isValidIdentifier(name)) {
        std::cout << "declare: `" << assignment << "': not a valid identifier" << std::endl;
        return;
      }
      
      setVariable(name, value);
    }
  }
}

// Runs ONE builtin synchronously in the current process (used only for
// the single-command, non-piped case — see note in executePipeline).
static bool runBuiltinDispatch(const Command& cmd) {
  if (cmd.name == "exit") return false;
  if (cmd.name == "echo") { runEcho(cmd); return true; }
  if (cmd.name == "pwd") { runPwd(); return true; }
  if (cmd.name == "cd") { runCd(cmd); return true; }
  if (cmd.name == "complete") { runComplete(cmd); return true; }
  if (cmd.name == "jobs") { runJobs(cmd); return true; }
  if (cmd.name == "history") { runHistory(cmd); return true; }
  if (cmd.name == "declare") { runDeclare(cmd); return true; }
  if (cmd.name == "type") { runType(cmd); return true; }
  return true;
}

// Applies this command's redirections via dup2. Called in a CHILD
// process, after fork(), before execvp(). pipeIn/pipeOut are the fds
// to use for stdin/stdout when part of a pipeline (-1 means "leave as
// inherited", i.e. no pipe on that side).
static void setupChildIO(const Command& cmd, int pipeIn, int pipeOut) {
    if (pipeIn != -1) {
        dup2(pipeIn, STDIN_FILENO);
    }
    if (pipeOut != -1) {
        dup2(pipeOut, STDOUT_FILENO);
    }

    if (!cmd.stdout_redirect.empty()) {
        int flags = O_WRONLY | O_CREAT | (cmd.stdout_append ? O_APPEND : O_TRUNC);
        int fd = open(cmd.stdout_redirect.c_str(), flags, 0644);
        if (fd < 0) { perror("open"); _exit(1); }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }

    if (!cmd.stderr_redirect.empty()) {
        int flags = O_WRONLY | O_CREAT | (cmd.stderr_append ? O_APPEND : O_TRUNC);
        int fd = open(cmd.stderr_redirect.c_str(), flags, 0644);
        if (fd < 0) { perror("open"); _exit(1); }
        dup2(fd, STDERR_FILENO);
        close(fd);
    }
}

// Forks one child to run a single external command as part of a
// pipeline. Does NOT wait — caller collects all pids and waits after
// launching every stage.
static pid_t spawnPipelineStage(const Command& cmd, int pipeIn, int pipeOut) {
  pid_t pid = fork();

  if (pid < 0) {
    perror("fork");
    return -1;
  }

  if (pid == 0) {
    signal(SIGINT, SIG_DFL);  // child gets normal Ctrl+C behavior back

    setupChildIO(cmd, pipeIn, pipeOut);

    if (isBuiltinName(cmd.name)) {
      // Run the builtin's logic directly in this forked child.
      // Its stdout/stdin are already wired to the pipe via
      // setupChildIO's dup2 calls above, so std::cout/std::cin
      // naturally flow through the pipeline correctly.
      runBuiltinDispatch(cmd);
      std::cout.flush();
      _exit(0);
    }

    std::vector<char*> argv;
    argv.push_back(const_cast<char*>(cmd.name.c_str()));
    for (auto& a : cmd.args) argv.push_back(const_cast<char*>(a.c_str()));
   
    argv.push_back(nullptr);

    execvp(cmd.name.c_str(), argv.data());

    std::cerr << cmd.name << ": command not found" << std::endl;
    _exit(127);
  }

  return pid;
}

static std::string reconstructPipelineCommandLine(const Pipeline& pipeline) {
  std::string line;
  for (size_t i = 0; i < pipeline.commands.size(); ++i) {
    const Command& cmd = pipeline.commands[i];
    line += cmd.name;
    for (const auto& arg : cmd.args) line += " " + arg;
    if (i + 1 < pipeline.commands.size()) line += " | ";
  }
  return line;
}

bool executePipeline(const Pipeline& pipeline) {
  if (pipeline.commands.empty()) {
    return true;
  }

  // Single command, no pipe involved: keep the existing behavior —
  // builtins run directly in this process (so `cd`, `exit` etc. still
  // affect the shell itself), externals fork/exec as before.
  // (Builtins INSIDE a multi-stage pipeline are out of scope for this
  // stage — that's the "Pipelines with built-ins" stage next.)
  if (pipeline.commands.size() == 1) {
    const Command& cmd = pipeline.commands[0];

    if (cmd.name.empty()) return true;

    if (isBuiltinName(cmd.name) || cmd.name == "exit") {
      // Redirect std::cout/std::cerr for the duration of this
      // builtin call, same mechanism as before the pipeline
      // refactor — builtins write via iostreams, not raw fds,
      // so dup2 alone (used for external commands) doesn't
      // affect them.
      std::ofstream outFile, errFile;
      std::streambuf* origCoutBuf = nullptr;
      std::streambuf* origCerrBuf = nullptr;

      if (!cmd.stdout_redirect.empty()) {
        auto mode = cmd.stdout_append ? std::ios::app : std::ios::trunc;
        outFile.open(cmd.stdout_redirect, mode);
        if (outFile.is_open()) {
          origCoutBuf = std::cout.rdbuf(outFile.rdbuf());
        }
      }
      if (!cmd.stderr_redirect.empty()) {
        auto mode = cmd.stderr_append ? std::ios::app : std::ios::trunc;
        errFile.open(cmd.stderr_redirect, mode);
        if (errFile.is_open()) {
          origCerrBuf = std::cerr.rdbuf(errFile.rdbuf());
        }
      }

      bool result = runBuiltinDispatch(cmd);

      if (origCoutBuf) std::cout.rdbuf(origCoutBuf);
      if (origCerrBuf) std::cerr.rdbuf(origCerrBuf);

      return result;
    }

    const char* env_path = std::getenv("PATH");
    fs::path command_path = env_path ? findExecutableFilePath(env_path, cmd.name) : fs::path{};

    if (command_path.empty()) {
      std::cout << cmd.name << ": command not found" << std::endl;
      return true;
    }

    pid_t pid = spawnPipelineStage(cmd, -1, -1);
    if (pid < 0) return true;

    if (cmd.background) {
      int jobNumber = registerBackgroundJob(pid, reconstructPipelineCommandLine(pipeline));
      std::cout << "[" << jobNumber << "] " << pid << std::endl;
    } else {
      int status;
      waitpid(pid, &status, 0);
    }

    return true;
  }

  // Multi-command pipeline: chain N commands via N-1 pipes.
  size_t n = pipeline.commands.size();
  std::vector<pid_t> pids;

  int prevReadEnd = -1; // stdin source for the NEXT command, from the PREVIOUS pipe

  for (size_t i = 0; i < n; ++i) {
    int pipefd[2] = {-1, -1};
    bool isLast = (i == n - 1);

    if (!isLast) {
      if (pipe(pipefd) != 0) {
        perror("pipe");
        break;
      }
    }

    int stageIn = prevReadEnd;                 // -1 for the first command
    int stageOut = isLast ? -1 : pipefd[1];     // -1 for the last command

    pid_t pid = spawnPipelineStage(pipeline.commands[i], stageIn, stageOut);
    if (pid > 0) pids.push_back(pid);

    // Parent no longer needs these ends once the child has them
    // (the child inherited copies via fork(), dup2'd its own).
    if (prevReadEnd != -1) close(prevReadEnd);
    if (!isLast) close(pipefd[1]);

    prevReadEnd = isLast ? -1 : pipefd[0]; // becomes next stage's stdin source
  }

  for (pid_t pid : pids) {
    int status;
    waitpid(pid, &status, 0);
  }

  return true;
}

// bool executeCommand(const Command& cmd) {
//   if (cmd.name.empty()) {
//     return true;  // empty input, just reprompt
//   }

//   if (cmd.name == "exit") {
//     return false;
//   }

//   bool isBuiltinCmd = (cmd.name == "echo" || cmd.name == "pwd" 
//                        || cmd.name == "type" || cmd.name == "cd"
//                        || cmd.name == "complete" || cmd.name == "jobs");

//   // Builtins write through std::cout
//   // redirect by swapping stream buffer and then restoring it
//   std::ofstream outFile, errFile;
//   std::streambuf* origCoutBuf = nullptr;
//   std::streambuf* origCerrBuf = nullptr;

//   if (isBuiltinCmd) {

//     if (!cmd.stdout_redirect.empty()) {
//       auto mode = cmd.stdout_append ? std::ios::app : std::ios::trunc;
//       outFile.open(cmd.stdout_redirect, mode);
//       if (outFile.is_open()) {
//         // changes cout's buffer to file's buffer and returns old buffer
//         origCoutBuf = std::cout.rdbuf(outFile.rdbuf());
//       }
//     }
//     if (!cmd.stderr_redirect.empty()) {
//       auto mode = cmd.stderr_append ? std::ios::app : std::ios::trunc;
//       errFile.open(cmd.stderr_redirect, mode);
//       if (errFile.is_open()) {
//         // changes cerr's buffer to file's buffer and returns old buffer
//         origCerrBuf = std::cerr.rdbuf(errFile.rdbuf());
//       }
//     }
//   }

//   bool result = true;

//   if (cmd.name == "echo") {
//     runEcho(cmd);
//   } else if (cmd.name == "pwd") {
//     runPwd();
//   } else if (cmd.name == "cd") {
//     runCd(cmd);
//   } else if (cmd.name == "type") {
//     runType(cmd);
//   } else if (cmd.name == "complete") {
//     runComplete(cmd);
//   } else if (cmd.name == "jobs") {
//     runJobs(cmd);
//   } else { 
//     runExternal(cmd);
//   }

//   if (origCoutBuf) {
//     std::cout.rdbuf(origCoutBuf);   // restore terminal output
//   }
//   if (origCerrBuf) {
//     std::cerr.rdbuf(origCerrBuf);
//   }

//   return result;
// }