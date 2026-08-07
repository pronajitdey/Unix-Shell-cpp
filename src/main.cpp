#include <iostream>
#include <string>

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
    
    std::getline(std::cin, input);

    size_t first_space = input.find(' ');
    if (first_space != std::string::npos) {
      command = input.substr(0, first_space);
      arguments = input.substr(first_space + 1);
    } else {
      command = input;
      arguments = "";
    }

    if (command == "exit") break;
    else if (command == "echo") {
      std::cout << arguments << std::endl;
    } else if (command == "type ") {
      if (arguments == "echo" || arguments == "exit" || arguments == "type") {
        std::cout << arguments << " is a shell builtin" << std::endl;
      } else {
        std::cout << arguments << ": not found" << std::endl;
      }
    } else {
      std::cout << command << ": command not found" << std::endl;
    }
  }
}
