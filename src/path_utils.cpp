#include "shell/path_utils.h"

#ifdef _WIN32
  const char PATH_SEPARATOR = ';';
#else
  const char PATH_SEPARATOR = ':';
#endif

// find path of file from PATH environment
fs::path findExecutableFilePath(const char* env_path, const std::string& file) {
  if (env_path == nullptr) return fs::path{};

  std::string env_path_string(env_path);

  while (env_path_string.find(PATH_SEPARATOR) != std::string::npos) {
    size_t separator_index = env_path_string.find(PATH_SEPARATOR);
    std::string search_dir = env_path_string.substr(0, separator_index);
    fs::path file_path = (fs::path)search_dir / file;

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
bool isFileExecutable(const fs::path& file_path) {
  if (fs::exists(file_path)) {
    fs::file_status status = fs::status(file_path);
    fs::perms permissions = status.permissions();
    
    bool executable = 
                (permissions & (fs::perms::owner_exec
                                | fs::perms::group_exec
                                | fs::perms::others_exec))
                != fs::perms::none;
    return executable;
  }
  return false;
}