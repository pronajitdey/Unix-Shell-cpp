#pragma once
#include <string>
#include <vector>

// Records one executed command line, in order. Called once per
// non-empty line read from the prompt, right after readline() returns
// and before parsing/execution — so it captures exactly what the user
// typed, including "history" itself and invalid commands.
void addHistoryEntry(const std::string& line);

// Returns all recorded entries in execution order (1-indexed display
// numbering is applied by the caller, e.g. runHistory).
const std::vector<std::string>& getHistory();