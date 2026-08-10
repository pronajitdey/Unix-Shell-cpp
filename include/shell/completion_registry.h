#pragma once
#include <string>
#include <unordered_map>

// Shared between executor.cpp (which populates it via `complete -C`)
// and completion.cpp (which reads it on TAB). A function returning a
// reference to a function-local static keeps it a single instance
// without needing a global variable declared at namespace scope.
std::unordered_map<std::string, std::string>& completionRegistry();
