#include "shell/history.h"

#include <readline/history.h>
#include <fstream>

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

bool loadHistoryFromFile(const std::string& path) {
  std::ifstream file(path);

  if (!file.is_open()) {
    return false; // do nothing if the file can't be opened
  }

  std::string line;
  while (std::getline(file, line)) {
    addHistoryEntry(line);  // empty lines are already skipped inside addHistoryEntry
    add_history(line.c_str());  // keep readline's arrow-key recall in sync too
  }

  setLastAppendedIndex(getHistory().size());

  return true;
}

bool saveHistoryToFile(const std::string& path) {
  std::ofstream file(path);

  if (!file.is_open()) {
    return false;
  }

  for (const auto& entry : getHistory()) {
    file << entry << "\n";
  }

  setLastAppendedIndex(getHistory().size());
  return true;
}

bool appendHistoryToFile(const std::string& path) {
  std::ofstream file(path, std::ios::app);

  if (!file.is_open()) {
    return false;
  }

  const auto& history = getHistory();
  size_t start = getLastAppendedIndex();

  for (size_t i = start; i < history.size(); ++i) {
    file << history[i] << "\n";
  }

  setLastAppendedIndex(history.size());
  return true;
}