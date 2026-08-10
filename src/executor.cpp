#include "shell/executor.h"
#include "shell/path_utils.h"
#include "shell/completion_registry.h"

#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include <sys/wait.h>
#include <fstream>
#include <fcntl.h>
#include <unordered_map>

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
      || target == "complete") {    // check builtin commands
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
static void runExternal(const Command& cmd) {
  const char* env_path = std::getenv("PATH");
  fs::path command_path = env_path ? findExecutableFilePath(env_path, cmd.name) : fs::path{};

  if (command_path.empty()) {
    std::cout << cmd.name << ": command not found" << std::endl;
    return;
  }

  pid_t pid = fork();

  if (pid < 0) {
    perror("fork");
    return;
  }

  if (pid == 0) {
    if (!cmd.stdout_redirect.empty()) {
      int flags = O_WRONLY | O_CREAT | (cmd.stdout_append ? O_APPEND : O_TRUNC);
      int fd = open(cmd.stdout_redirect.c_str(), flags, 0644);

      if (fd < 0) {
        perror("open");
        _exit(1);
      }
      // only child's file descriptor is changed
      dup2(fd, STDOUT_FILENO);
      close(fd);
    }

    if (!cmd.stderr_redirect.empty()) {
      int flags = O_WRONLY | O_CREAT | (cmd.stderr_append ? O_APPEND : O_TRUNC);
      int fd = open(cmd.stderr_redirect.c_str(), flags, 0644);

      if (fd < 0) { 
        perror("open"); 
        _exit(1); 
      }

      dup2(fd, STDERR_FILENO);
      close(fd);
    }

    std::vector<char*> argv;
    argv.push_back(const_cast<char*>(cmd.name.c_str()));
    for (auto& a : cmd.args) argv.push_back(const_cast<char*>(a.c_str()));
    argv.push_back(nullptr);

    // new program inherits the child's file descriptors
    execvp(cmd.name.c_str(), argv.data());

    // only reached if execvp fails
    perror("execvp");
    _exit(127);   // 127 for command not found (in shell scripts)
  } else {
    int status;
    waitpid(pid, &status, 0);
  }
}

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

bool executeCommand(const Command& cmd) {
  if (cmd.name.empty()) {
    return true;  // empty input, just reprompt
  }

  if (cmd.name == "exit") {
    return false;
  }

  bool isBuiltinCmd = (cmd.name == "echo" || cmd.name == "pwd" 
                       || cmd.name == "type" || cmd.name == "cd"
                       || cmd.name == "complete");

  // Builtins write through std::cout
  // redirect by swapping stream buffer and then restoring it
  std::ofstream outFile, errFile;
  std::streambuf* origCoutBuf = nullptr;
  std::streambuf* origCerrBuf = nullptr;

  if (isBuiltinCmd) {

    if (!cmd.stdout_redirect.empty()) {
      auto mode = cmd.stdout_append ? std::ios::app : std::ios::trunc;
      outFile.open(cmd.stdout_redirect, mode);
      if (outFile.is_open()) {
        // changes cout's buffer to file's buffer and returns old buffer
        origCoutBuf = std::cout.rdbuf(outFile.rdbuf());
      }
    }
    if (!cmd.stderr_redirect.empty()) {
      auto mode = cmd.stderr_append ? std::ios::app : std::ios::trunc;
      errFile.open(cmd.stderr_redirect, mode);
      if (errFile.is_open()) {
        // changes cerr's buffer to file's buffer and returns old buffer
        origCerrBuf = std::cerr.rdbuf(errFile.rdbuf());
      }
    }
  }

  bool result = true;

  if (cmd.name == "echo") {
    runEcho(cmd);
  } else if (cmd.name == "pwd") {
    runPwd();
  } else if (cmd.name == "cd") {
    runCd(cmd);
  } else if (cmd.name == "type") {
    runType(cmd);
  } else if (cmd.name == "complete") {
    runComplete(cmd);
  } else { 
    runExternal(cmd);
  }

  if (origCoutBuf) {
    std::cout.rdbuf(origCoutBuf);   // restore terminal output
  }
  if (origCerrBuf) {
    std::cerr.rdbuf(origCerrBuf);
  }

  return result;
}