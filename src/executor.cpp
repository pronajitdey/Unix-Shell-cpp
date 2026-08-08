#include "shell/executor.h"
#include "shell/path_utils.h"

#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include <sys/wait.h>

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
      || target == "cd") {    // check builtin commands
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
    std::vector<char*> argv;
    argv.push_back(const_cast<char*>(cmd.name.c_str()));
    for (auto& a : cmd.args) argv.push_back(const_cast<char*>(a.c_str()));
    argv.push_back(nullptr);

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

  const std::string& target = cmd.args[0];
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

bool executeCommand(const Command& cmd) {
  if (cmd.name.empty()) {
    return true;  // empty input, just reprompt
  }

  if (cmd.name == "exit") {
    return false;
  }

  if (cmd.name == "echo") {
    runEcho(cmd);
    return true;
  }

  if (cmd.name == "pwd") {
    runPwd();
    return true;
  }

  if (cmd.name == "cd") {
    runCd(cmd);
    return true;
  }

  if (cmd.name == "type") {
    runType(cmd);
    return true;
  }

  runExternal(cmd);
  return true;
}