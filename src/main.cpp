#include <iostream>
#include <cstdlib>
#include <readline/readline.h>
#include <readline/history.h>

#include "shell/command.h"
#include "shell/parser.h"
#include "shell/executor.h"
#include "shell/completion.h"
#include "shell/history.h"

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  initCompletion();

  while (true) {
    reapAndAnnounceFinishedJobs(); // check before every prompt

    char* line = readline("$ ");

    if (line == nullptr) {
      break; // Ctrl+D / EOF
    }

    std::string input(line);
    free(line);

    // record BEFORE execution, so "history"
    // itself and failed commands are included
    addHistoryEntry(input);

    // Command cmd = parseCommand(input);
    Pipeline pipeline = parsePipeline(input);

    if (!executePipeline(pipeline)) {
      break;  // "exit" entered
    }
  }

  return 0;
}


