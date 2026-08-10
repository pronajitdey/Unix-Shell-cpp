#pragma once
#include <string>
#include <vector>

struct Command {
  std::string name;
  std::vector<std::string> args;  // does NOT include name

  std::string stdout_redirect;  // empty if no redirection
  bool stdout_append = false;   // true for >> / 1>>

  std::string stderr_redirect;  // empty if no redirection
  bool stderr_append = false;   // true for 2>>

  bool background = false;  // true if line ends with '&
};