#pragma once
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

// find path of file from PATH environment
fs::path findExecutableFilePath(const char* env_path, const std::string& file);

// check whether a file is executable or not
bool isFileExecutable(const fs::path& file_path);