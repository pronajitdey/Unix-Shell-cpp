#pragma once
#include <string>

// Sets (or overwrites) a shell variable.
void setVariable(const std::string& name, const std::string& value);

// Returns true and fills `outValue` if the variable exists.
bool getVariable(const std::string& name, std::string& outValue);
