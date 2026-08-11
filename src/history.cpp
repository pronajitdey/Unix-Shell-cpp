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