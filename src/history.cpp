#include "shell/history.h"

static std::vector<std::string>& historyList() {
  static std::vector<std::string> entries;
  return entries;
}

void addHistoryEntry(const std::string& line) {
  if (line.empty()) return; // don't record blank Enter presses
  historyList().push_back(line);
}

const std::vector<std::string>& getHistory() {
  return historyList();
}

static size_t& lastAppendedIndexRef() {
  static size_t index = 0;
  return index;
}

size_t getLastAppendedIndex() {
  return lastAppendedIndexRef();
}

void setLastAppendedIndex(size_t index) {
  lastAppendedIndexRef() = index;
}
