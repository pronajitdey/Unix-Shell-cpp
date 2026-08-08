#pragma once
#include "shell/command.h"

// Returns false if the shell should exit ("exit" command)
bool executeCommand(const Command& cmd);
