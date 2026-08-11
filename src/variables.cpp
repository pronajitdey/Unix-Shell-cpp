#include "shell/variables.h"
#include <unordered_map>

static std::unordered_map<std::string, std::string>& variableStore() {
    static std::unordered_map<std::string, std::string> vars;
    return vars;
}

void setVariable(const std::string& name, const std::string& value) {
    variableStore()[name] = value;
}

bool getVariable(const std::string& name, std::string& outValue) {
    auto it = variableStore().find(name);
    if (it == variableStore().end()) {
        return false;
    }
    outValue = it->second;
    return true;
}
