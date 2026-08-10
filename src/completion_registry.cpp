#include "shell/completion_registry.h"

std::unordered_map<std::string, std::string>& completionRegistry() {
    static std::unordered_map<std::string, std::string> registry;
    return registry;
}