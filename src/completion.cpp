#include "shell/completion.h"

#include <readline/readline.h>
#include <cstring>
#include <string>
#include <vector>

static const std::vector<std::string> BUILTIN_NAMES = {
  "echo", "exit"
};

// Called repeatedly by readline for each TAB press. state==0 starts a
// fresh search; state>0 asks for the next match after the previous call.
static char* builtinGenerator(const char* text, int state) {
  static size_t index;
  static std::string prefix;

  if (state == 0) {
    index = 0;
    prefix = text;
  }

  while (index < BUILTIN_NAMES.size()) {
    const std::string& candidate = BUILTIN_NAMES[index++];
    if (candidate.compare(0, prefix.size(), prefix) == 0) {
      return strdup(candidate.c_str()); // readline frees this itself
    }
  }

  return nullptr; // no more matches
}

static char** shellCompletion(const char* text, int start, int end) {
  (void)end;

  if (start != 0) {
    return nullptr; // only complete the command name, not arguments
  }

  rl_attempted_completion_over = 1; // don't fall back to filename completion
  return rl_completion_matches(text, builtinGenerator);
}

void initCompletion() {
  rl_attempted_completion_function = shellCompletion;
}