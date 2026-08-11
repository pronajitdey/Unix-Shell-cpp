#pragma once
#include <sys/types.h>
#include <string>
#include <vector>

struct Job {
  int number;
  pid_t pid;
  std::string commandLine;  // doesn't include trailing '&'
  bool running; // true = Running, false = Done
};

// Registers a new background job, returns its assigned job number.
int registerBackgroundJob(pid_t pid, const std::string& commandLine);

// Returns all currently tracked jobs, in registration order
const std::vector<Job>& allJobs();

// Checks each tracked job with waitpid(..., WNOHANG) to see if it has
// exited, marking it as no longer running. Does not remove entries —
// that's the caller's responsibility (so callers can display Done
// status once before removing it).
void reapFinishedJobs();

// Removes all jobs currently marked as not running from the table.
void removeFinishedJobs();
