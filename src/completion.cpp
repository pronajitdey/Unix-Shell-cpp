#include "shell/completion.h"
#include "shell/path_utils.h"
#include "shell/completion_registry.h"

#include <readline/readline.h>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>
#include <unordered_set>
#include <algorithm>
#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <sstream>
#include <cstdio> // for snprintf

#ifdef _WIN32
  const char PATH_SEPARATOR = ';';
#else
  const char PATH_SEPARATOR = ':';
#endif

namespace fs = std::filesystem;

static const std::vector<std::string> BUILTIN_NAMES = {
  "echo", "exit", "completion"
};

// command-name completion (builtins + PATH executables)
// Scans $PATH once and returns the set of all matching candidate names
// (builtins + every executable file found across PATH directories).
// Rebuilding on every TAB press is intentional for now — it guarantees
// freshness if PATH or the filesystem changes mid-session; if this ever
// feels slow, cache it and invalidate on demand.
static std::vector<std::string> collectCommandCandidates(const std::string& prefix) {
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

  std::sort(results.begin(), results.end());
  return results;
}

// filename completion (in current directory with partial paths)
static std::vector<std::string> collectFilenameCandidates(const std::string& text) {
  std::vector<std::string> results;

  size_t lastSlash = text.find_last_of('/');

  fs::path searchDir;
  std::string prefix;
  std::string dirPart;  // "" if no slash

  if (lastSlash == std::string::npos) {
    // No '/' in filename, search current directory
    searchDir = fs::current_path();
    prefix = text;
  } else {
    // split into "directory part" (up to and including the last '/')
    // and "prefix part" (everything after it)
    dirPart = text.substr(0, lastSlash + 1);
    searchDir = dirPart;
    prefix = text.substr(lastSlash + 1);
  }

  std::error_code ec;
  if (!fs::is_directory(searchDir, ec)) {
    return results; // directory part doesn't exist
  }

  for (const auto& entry : fs::directory_iterator(searchDir, ec)) {
    if (ec) break;

    const std::string filename = entry.path().filename().string();

    if (filename.compare(0, prefix.length(), prefix) == 0) {
      // return full token (directory part + prefix part),
      // since readline replaces the entire word with what we return
      results.push_back(dirPart + filename);
    }
  }

  std::sort(results.begin(), results.end());
  return results;
}

// Registered completer scripts
// Runs `scriptPath` as a child process with no arguments (this stage
// doesn't pass completion context yet), captures its stdout via a pipe,
// and splits it into lines. Blocks until the child exits, so output is
// always complete before we read it.
static std::vector<std::string> runCompleterScript(
    const std::string& script_path,
    const std::string& command_name,
    const std::string& word_being_completed,
    const std::string& previous_word,
    const std::string& compLine,
    int compPoint) {

  std::vector<std::string> lines;

  int pipefd[2];
  if (pipe(pipefd) != 0) {
    return lines;
  }

  pid_t pid = fork();

  if (pid < 0) {
    close(pipefd[0]);
    close(pipefd[1]);
    return lines;
  }

  if (pid == 0) {
    // Child: redirect stdout into the pipe's write end, then exec
    close(pipefd[0]);
    dup2(pipefd[1], STDOUT_FILENO);
    close(pipefd[1]);

    // setenv() here only affects THIS child's environment (post-fork,
    // separate address space) — the parent shell's environment is
    // completely untouched.
    setenv("COMP_LINE", compLine.c_str(), 1);

    char pointBuf[32];
    snprintf(pointBuf, sizeof(pointBuf), "%d", compPoint);
    setenv("COMP_POINT", pointBuf, 1);

    execl(script_path.c_str(), 
          script_path.c_str(),          // argv[0]: program name
          command_name.c_str(),         // argv[1]: command being completed
          word_being_completed.c_str(), // argv[2]: current partial word
          previous_word.c_str(),        // argv[3]: preceding word
          static_cast<char*>(nullptr));

    _exit(127);
  }

  // Parent: read everything the child writes, then wait for it to finish
  close(pipefd[1]);

  std::string output;
  char buffer[4096];
  ssize_t bytesRead;

  while ((bytesRead = read(pipefd[0], buffer, sizeof(buffer))) > 0) {
    output.append(buffer, bytesRead);
  }
  close(pipefd[0]);

  int status;
   // ensure script has fully finished and child has been reaped
  waitpid(pid, &status, 0);

  std::istringstream iss(output);
  std::string line;
  while (std::getline(iss, line)) {
    if (!line.empty()) lines.push_back(line);
  }

  return lines;
}

// Set just before calling rl_completion_matches(text, scriptGenerator),
// since the generator's signature (fixed by readline) can't take extra
// parameters directly.
static std::string g_pendingScriptPath;
static std::string g_pendingCommandPath;
static std::string g_pendingPreviousWord;
static std::string g_pendingCompLine;
static int g_pendingCompPoint;

static std::vector<std::string> collectScriptCandidates(const std::string& prefix) {  
  std::vector<std::string> results;
  std::vector<std::string> lines = runCompleterScript(g_pendingScriptPath, g_pendingCommandPath, prefix, g_pendingPreviousWord, g_pendingCompLine, g_pendingCompPoint);

  for (const auto& line : lines) {
    if (line.compare(0, prefix.size(), prefix) == 0) {
      results.push_back(line);
    }
  }

  return results;
}

// both generators share this, decided by shellCompletion based on cursor position (start == 0 or not)
static char* makeGenerator(const char* text, int state, std::vector<std::string> (*collector)(const std::string&)) {
  static std::vector<std::string> matches;
  static size_t index;

  if (state == 0) {
    matches = collector(text);
    index = 0;
  }

  if (index < matches.size()) {
    return strdup(matches[index++].c_str());
  }

  return nullptr;
}

// static char* candidateGenerator(const char* text, int state) {
//   static std::vector<std::string> matches;
//   static size_t index;

//   if (state == 0) {
//     matches = collectCommandCandidates(text);
//     index = 0;
//   }

//   if (index < matches.size()) {
//     return strdup(matches[index++].c_str());
//   }

//   return nullptr;
// }

static char* commandGenerator(const char* text, int state) {
  return makeGenerator(text, state, collectCommandCandidates);
}

static char* filenameGenerator(const char* text, int state) {
  return makeGenerator(text, state, collectFilenameCandidates);
}

static char* scriptGenerator(const char* text, int state) {
  return makeGenerator(text, state, collectScriptCandidates);
}

// custom display hook for filename/directory listing
// called whenever there is more than one match
// matches[0] is the computed longest-common-prefix whenever multiple matches,
// so our candidates are matches[1..len]
static void filenameDisplayHook(char** matches, int len, int max) {
  std::cout << "\n";

  for (int i = 1; i <= len; i++) {
    std::string entry = matches[i];

    std::error_code ec;
    if (fs::is_directory(entry, ec)) {
      entry += "/";
    }

    std::cout << entry;
    if (i < len) std::cout << "  ";
  }

  std::cout << "\n";
  rl_forced_update_display(); // redraw "$ <original input>" below the listing
}

// extract the command name (first word) from the line
static std::string extractCommandName() {
  std::string line(rl_line_buffer ? rl_line_buffer : "");
  size_t firstSpace = line.find(' ');
  return (firstSpace == std::string::npos) ? line : line.substr(0, firstSpace);
}

// Everything before the word currently being completed (i.e. text[0..start))
// in the full line, then take the last whitespace-delimited token of that.
static std::string extractPreviousWord(int start) {
  if (rl_line_buffer == nullptr || start <= 0) {
    return "";
  }

  std::string before_cursor(rl_line_buffer, start); // chars [0, start)

  // Trim trailing whitespace (the space right before the word being completed)
  size_t end = before_cursor.find_last_not_of(' ');
  if (end == std::string::npos) return "";
  before_cursor = before_cursor.substr(0, end + 1);

  // Now find the last remaining space to isolate the final token
  size_t lastSpace = before_cursor.find_last_of(' ');
  if (lastSpace == std::string::npos) {
    // Only one token before the cursor - that's the command name itself,
    // so no argument before the one being completed
    return "";
  }
  
  return before_cursor.substr(lastSpace + 1);
}

// readline parses word boundaries itself using its default word-break characters
// which includes space (so extracts text after the last space)
static char** shellCompletion(const char* text, int start, int end) {
    (void)end;

  rl_attempted_completion_over = 1;
  rl_completion_append_character = ' ';   // reset to default each time

  char** matches;

  if (start == 0) {
    rl_completion_display_matches_hook = nullptr; // default listing for commands
    matches = rl_completion_matches(text, commandGenerator);
  } else {
    std::string command_name = extractCommandName();
    auto& registry = completionRegistry();
    auto it = registry.find(command_name);

    if (it != registry.end()) {
      // completer script is registered for this command
      // use it instead of filename completion
      g_pendingScriptPath = it->second;
      g_pendingCommandPath = command_name;
      g_pendingPreviousWord = extractPreviousWord(start);

      // COMP_LINE: the full line as readline currently holds it.
      // COMP_POINT: cursor position, which readline tracks in rl_point.
      g_pendingCompLine = (rl_line_buffer != nullptr) ? std::string(rl_line_buffer) : "";

      // rl_point is not necessarily equal to end. 
      // For example, if the cursor is in the middle of a word, 
      // rl_point can be between start and end.
      g_pendingCompPoint = rl_point;

      rl_completion_display_matches_hook = nullptr;
      matches = rl_completion_matches(text, scriptGenerator);
    } else {
      rl_completion_display_matches_hook = filenameDisplayHook;
      matches = rl_completion_matches(text, filenameGenerator);
      
      // If exactly one match and it's a directory, append '/' instead of a space, and suppress the space entirely
      if (matches != nullptr && matches[0] != nullptr && matches[1] == nullptr) {
        std::error_code ec;
        if (fs::is_directory(matches[0], ec)) {
          rl_completion_append_character = '/';
        }
      }
    }
  }

  return matches;
}

void initCompletion() {
  rl_attempted_completion_function = shellCompletion;
  rl_variable_bind("bell-style", "audible");
}