#include <iostream>
#include "shell/command.h"
#include "shell/parser.h"
#include "shell/executor.h"

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;
  
  std::string input;
  std::string command;
  std::string arguments;

  while (true) {
    // TODO: Uncomment the code below to pass the first stage
    std::cout << "$ ";
    
    if (!std::getline(std::cin, input)) break;  // EOF / Ctrl+D

    Command cmd = parseCommand(input);

    if (!executeCommand(cmd)) {
      break;  // "exit" entered
    }
  }

  return 0;
}


