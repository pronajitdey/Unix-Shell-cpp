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

    if (c == '"') {
      inToken = true;
      i++;
      while (i < n && input[i] != '"') {
        if (input[i] == '\\' && i + 1 < n
            && (input[i + 1] == '"' || input[i + 1] == '\\'
                || input[i + 1] == '$' || input[i + 1] == '`')) {

          current += input[i + 1];
          i += 2;
        } else { 
          current += input[i];
          i++;
        }
      }
      if (i < n) i++;
      continue;
    }

    if (c == '\\') {
      // Outside quotes, backlash escapes the next character
      inToken = true;
      i++;  // skip the backlash
      if (i < n) {
        current += input[i];
        i++;
      }
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

  if (tokens.empty()) return cmd;

  // strip redirection tokens out of token stream
  std::vector<std::string> filtered;

  for (size_t i = 0; i < tokens.size(); i++) {
    const std::string& tok = tokens[i];

    if (tok == ">" || tok == "1>") {
      if (i + 1 < tokens.size()) {
        cmd.stdout_redirect = tokens[i + 1];
        i++;
      }
      continue;
    }

    filtered.push_back(tok);
  }

  if (!filtered.empty()) {
    cmd.name = filtered[0];
    cmd.args.assign(filtered.begin() + 1, filtered.end());
  }

  return cmd;
}