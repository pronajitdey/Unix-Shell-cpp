#pragma once
#include <string>
#include <vector>

struct Command {
  std::string name;
  std::vector<std::string> args;  // does NOT include name
  std::string stdout_redirect;  // empty if no redirection
};