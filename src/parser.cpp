#include "shell/parser.h"
#include <sstream>

Command parseCommand(const std::string& input) {
  Command cmd;
  std::istringstream iss(input);
  std::string token;

  if (iss >> token) {
    cmd.name = token;
    while (iss >> token) {
      cmd.args.push_back(token);
    }
  }

  return cmd;
}