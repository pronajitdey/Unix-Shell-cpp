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

// Tracks how many entries have already been appended to a history file
// via `history -a`, so subsequent calls only write what's new since
// the last append. Shared across the whole session (not per-file) —
// matches bash's behavior of a single "last append point" cursor.
size_t getLastAppendedIndex();
void setLastAppendedIndex(size_t index);

// Reads a history file line-by-line, adding each non-empty line to
// both the in-memory history list and readline's own history. Used by
// both `history -r` and automatic HISTFILE loading on startup.
bool loadHistoryFromFile(const std::string& path);