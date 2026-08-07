#include <iostream>
#include <string>
#include <cstdlib>
#include <filesystem>

#ifdef _WIN32
  const char PATH_SEPARATOR = ';';
#else
  const char PATH_SEPARATOR = ':';
#endif

namespace fs = std::filesystem;

fs::path findExecutableFilePath(const char* env_path, std::string file);
bool isFileExecutable(const fs::path &file_path);

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

    const char* env_path = std::getenv("PATH");

    if (command == "exit") break;
    else if (command == "echo") {
      std::cout << arguments << std::endl;

    } else if (command == "type") {   // check type of a command
      if (arguments == "echo" || arguments == "exit" || arguments == "type") {    // check builtin commands
        std::cout << arguments << " is a shell builtin" << std::endl;

      } else {    // check command is executable file in PATH environment
        bool file_is_executable = false;

        if (env_path != NULL) {
          fs::path file_path = findExecutableFilePath(env_path, arguments);

          if (!file_path.empty()) {
            std::cout << arguments << " is " << file_path.string() << std::endl;
          } else {
            std::cout << arguments << ": not found" << std::endl;
          }

        } else {    // PATH environment not found
          std::cout << arguments << ": not found" << std::endl;
        }
      }
      
    } else {
      fs::path command_path = findExecutableFilePath(env_path, command);

      if (!command_path.empty()) {  // Run an executable program
        std::system(input.c_str());
      } else {
        std::cout << command << ": command not found" << std::endl;
      }
    }
  }
}

// find path of file from PATH environment
fs::path findExecutableFilePath(const char* env_path, std::string file) {
  std::string env_path_string(env_path);

  while (env_path_string.find(PATH_SEPARATOR) != std::string::npos) {
    size_t separator_index = env_path_string.find(PATH_SEPARATOR);
    std::string search_dir = env_path_string.substr(0, separator_index);
    fs::path file_path = search_dir + "/" + file;

    if (isFileExecutable(file_path)) {
      return file_path;
    }

    env_path_string = env_path_string.substr(separator_index + 1);
  }

  // last path in the PATH environment (no PATH SEPARATOR after it)           
  std::string search_dir = env_path_string;
  fs::path file_path = search_dir + "/" + file;

  if (isFileExecutable(file_path)) {
    return file_path;
  }

  return fs::path{};
}

// check whether a file is executable or not
bool isFileExecutable(const fs::path &file_path) {
  if (fs::exists(file_path)) {
    fs::file_status status = fs::status(file_path);
    fs::perms permissions = status.permissions();
    
    bool executable = (permissions & fs::perms::owner_exec) != fs::perms::none;
    return executable;
  }
  return false;
}
