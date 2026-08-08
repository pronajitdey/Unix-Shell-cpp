#include "shell/parser.h"
// #include <sstream>

static std::vector<std::string> tokenize(const std::string& input) {
  std::vector<std::string> tokens;
  std::string current;
  bool inToken = false;

  size_t i = 0;
  size_t n = input.size();

  while (i < n) {
    char c = input[i];

    if (c == ' ' || c == '\t') {
      if (inToken) {
        tokens.push_back(current);
        current.clear();
        inToken = false;
      }
      i++;
      continue;
    }

    if (c == '\'') {
      inToken = true;
      i++;
      while (i < n && input[i] != '\'') {
        current += input[i];
        i++;
      }
      if (i < n) i++;
      continue;
    }

    inToken = true;
    current += c;
    i++;
  }

  if (inToken) {
    tokens.push_back(current);
  }

  return tokens;
}

Command parseCommand(const std::string& input) {
  Command cmd;
  // std::istringstream iss(input);
  // std::string token;

  // if (iss >> token) {
  //   cmd.name = token;
  //   while (iss >> token) {
  //     cmd.args.push_back(token);
  //   }
  // }
  std::vector<std::string> tokens = tokenize(input);

  if (!tokens.empty()) {
    cmd.name = tokens[0];
    cmd.args.assign(tokens.begin() + 1, tokens.end());
  }

  return cmd;
}