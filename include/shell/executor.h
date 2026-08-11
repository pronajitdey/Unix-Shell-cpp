#pragma once
#include "shell/command.h"

// Returns false if the shell should exit ("exit" command)
// bool executeCommand(const Command& cmd);

// Executes a full pipeline (one or more commands connected by '|').
// Returns false if the shell should exit.
bool executePipeline(const Pipeline& pipeline);

// Reaps finished background jobs, printing a Done line for each and
// removing them from the table. Called after every command (including
// from main.cpp before printing the next prompt) and from the `jobs`
// builtin itself.
void reapAndAnnounceFinishedJobs();