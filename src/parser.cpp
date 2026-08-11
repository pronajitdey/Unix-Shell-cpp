#include "shell/parser.h"
#include "shell/variables.h"
// #include <sstream>

// Reads a variable name starting at input[i] (which must be '$'),
// looks it up, and returns its value (empty string if undefined).
// Advances `i` past the consumed "$NAME" text.
static std::string expandVariableAt(const std::string& input, size_t& i) {
  size_t start = i;
  i++;  // skip '$'

  // ${VAR} form: braces explicitly delimit the name
  if (i < input.size() && input[i] == '{') {
    size_t braceStart = i;
    i++;  // skip '{'
    
    size_t nameStart = i;
    while (i < input.size() && input[i] != '}') {
      i++;
    }
    
    if (i >= input.size()) {
      // '$' not followed by a valid identifier character — treat the
      // '$' itself as a literal character (no expansion possible).
      i = braceStart + 1;
      return "${";
    }

    std::string name = input.substr(nameStart, i - nameStart);
    i++; // skip '}'

    std::string value;
    if (getVariable(name, value)) {
      return value;
    }
    return "";
  }

  // $VAR form (no braces): read while alnum/underscore.
  size_t nameStart = i;
  while (i < input.size() &&
         (std::isalnum(static_cast<unsigned char>(input[i])) || input[i] == '_')) {
    i++;
  }

  if (i == nameStart) {
    i = start + 1;
    return "$";
  }

  std::string name = input.substr(nameStart, i - nameStart);
  std::string value;
  if (getVariable(name, value)) {
      return value;
  }
  return ""; // undefined variable expands to empty string, matching bash
}

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
        } else if (input[i] == '$') {
          current += expandVariableAt(input, i);
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

    if (c == '$') {
      inToken = true;
      current += expandVariableAt(input, i);
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

// Command parseCommand(const std::string& input) {
//   Command cmd;
//   // std::istringstream iss(input);
//   // std::string token;

//   // if (iss >> token) {
//   //   cmd.name = token;
//   //   while (iss >> token) {
//   //     cmd.args.push_back(token);
//   //   }
//   // }
//   std::vector<std::string> tokens = tokenize(input);

//   if (tokens.empty()) return cmd;

//   // Detect and strip a trailing '&' before redirection parsing
//   if (tokens.back() == "&") {
//     cmd.background = true;
//     tokens.pop_back();
//   }

//   // strip redirection tokens out of token stream
//   std::vector<std::string> filtered;

//   for (size_t i = 0; i < tokens.size(); i++) {
//     const std::string& tok = tokens[i];

//     if (tok == ">" || tok == "1>") {
//       if (i + 1 < tokens.size()) {
//         cmd.stdout_redirect = tokens[i + 1];
//         i++;
//       }
//       continue;
//     }

//     if (tok == ">>" || tok == "1>>") {
//       if (i + 1 < tokens.size()) {
//         cmd.stdout_redirect = tokens[i + 1];
//         cmd.stdout_append = true;
//         i++;
//       }
//       continue;
//     }

//     if (tok == "2>") {
//       if (i + 1 < tokens.size()) {
//         cmd.stderr_redirect = tokens[i + 1];
//         i++;
//       }
//       continue;
//     }

//     if (tok == "2>>") {
//       if (i + 1 < tokens.size()) {
//         cmd.stderr_redirect = tokens[i + 1];
//         cmd.stderr_append = true;
//         i++;
//       }
//       continue;
//     }

//     filtered.push_back(tok);
//   }

//   if (!filtered.empty()) {
//     cmd.name = filtered[0];
//     cmd.args.assign(filtered.begin() + 1, filtered.end());
//   }

//   return cmd;
// }

// Parses one already-split segment of tokens (between '|' boundaries)
// into a single Command, extracting redirection tokens as before.
static Command parseSingleCommand(const std::vector<std::string>& tokens) {
  Command cmd;
  std::vector<std::string> filtered;

  for (size_t i = 0; i < tokens.size(); ++i) {
    const std::string& tok = tokens[i];

    if (tok == ">" || tok == "1>") {
      if (i + 1 < tokens.size()) {
        cmd.stdout_redirect = tokens[i + 1];
        cmd.stdout_append = false;
        i++;
      }
      continue;
    }

    if (tok == ">>" || tok == "1>>") {
      if (i + 1 < tokens.size()) {
        cmd.stdout_redirect = tokens[i + 1];
        cmd.stdout_append = true;
        i++;
      }
      continue;
    }

    if (tok == "2>") {
      if (i + 1 < tokens.size()) {
        cmd.stderr_redirect = tokens[i + 1];
        cmd.stderr_append = false;
        i++;
      }
      continue;
    }

    if (tok == "2>>") {
      if (i + 1 < tokens.size()) {
        cmd.stderr_redirect = tokens[i + 1];
        cmd.stderr_append = true;
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

Pipeline parsePipeline(const std::string& input) {
  Pipeline pipeline;
  std::vector<std::string> tokens = tokenize(input);

  if (tokens.empty()) {
    return pipeline;
  }

  // Strip trailing '&' once, for the whole pipeline (background applies
  // to the pipeline as a unit, e.g. "cat f | wc &").
  bool background = false;
  if (tokens.back() == "&") {
    background = true;
    tokens.pop_back();
  }

  // Split remaining tokens into segments at each unquoted '|'.
  std::vector<std::vector<std::string>> segments;
  std::vector<std::string> current;

  for (const auto& tok : tokens) {
    if (tok == "|") {
      segments.push_back(current);
      current.clear();
    } else {
      current.push_back(tok);
    }
  }
  segments.push_back(current); // final segment after the last '|' (or the only one)

  for (const auto& segment : segments) {
    if (segment.empty()) continue; // guards against "cmd1 | | cmd2" style typos
    Command cmd = parseSingleCommand(segment);
    pipeline.commands.push_back(cmd);
  }

  if (!pipeline.commands.empty()) {
    pipeline.commands.back().background = background;
    // background applies to the whole pipeline, but we only need to
    // check it on the last command when deciding whether to wait
  }

  return pipeline;
}