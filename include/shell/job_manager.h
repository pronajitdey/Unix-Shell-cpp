#pragma once
#include <sys/types.h>
#include <string>

// Minimal job tracking, just enough to assign sequential job numbers
// and print "[N] PID" on background start. Listing/status tracking for
// the `jobs` builtin comes later.
int registerBackgroundJob(pid_t pid, const std::string& commandLine);
