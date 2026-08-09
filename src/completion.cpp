#include "shell/completion.h"
#include "shell/path_utils.h"

#include <readline/readline.h>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>
#include <unordered_set>

#ifdef _WIN32
  const char PATH_SEPARATOR = ';';
#else
  const char PATH_SEPARATOR = ':';
#endif

namespace fs = std::filesystem;

static const std::vector<std::string> BUILTIN_NAMES = {
  "echo", "exit"
};

// Scans $PATH once and returns the set of all matching candidate names
// (builtins + every executable file found across PATH directories).
// Rebuilding on every TAB press is intentional for now — it guarantees
// freshness if PATH or the filesystem changes mid-session; if this ever
// feels slow, cache it and invalidate on demand.
static std::vector<std::string> collectCandidates(const std::string& prefix) {
  std::unordered_set<std::string> unique; // de-dupe builtin vs PATH overlap
  std::vector<std::string> results;

  for (const auto& name : BUILTIN_NAMES) {
    if (name.compare(0, prefix.size(), prefix) == 0 && unique.insert(name).second) {
      results.push_back(name);
    }
  }

  const char* env_path = std::getenv("PATH");
  if (env_path != nullptr) {
    std::string path_string(env_path);
    size_t start = 0;

    while (start <= path_string.size()) {
      size_t sep = path_string.find(PATH_SEPARATOR, start);
      std::string dir = (sep == std::string::npos)
        ? path_string.substr(start)
        : path_string.substr(start, sep - start);

      if (!dir.empty()) {
        std::error_code ec;   // filesystem errors shouldn't throw exceptions
        if (fs::is_directory(dir, ec)) {
          for (const auto& entry : fs::directory_iterator(dir, ec)) {
            // entry is each file/directory inside dir
            if (ec) break; // directory vanished mid-iteration, etc.

            const std::string filename = entry.path().filename().string();

            if (filename.compare(0, prefix.size(), prefix) == 0 &&
              isFileExecutable(entry.path()) &&
              unique.insert(filename).second) {
              results.push_back(filename);
            }
          }
        }
        // if dir doesn't exist or isn't a directory, we just skip it —
        // handles "PATH can include directories that don't exist"
      }

      if (sep == std::string::npos) break;
      start = sep + 1;
    }
  }

  return results;
}

static char* candidateGenerator(const char* text, int state) {
  static std::vector<std::string> matches;
  static size_t index;

  if (state == 0) {
    matches = collectCandidates(text);
    index = 0;
  }

  if (index < matches.size()) {
    return strdup(matches[index++].c_str());
  }

  return nullptr;
}

static char** shellCompletion(const char* text, int start, int end) {
    (void)end;

  rl_attempted_completion_over = 1;

  if (start != 0) {
    return nullptr;
  }

  return rl_completion_matches(text, candidateGenerator);
}

void initCompletion() {
  rl_attempted_completion_function = shellCompletion;
  rl_variable_bind("bell-style", "audible");
}