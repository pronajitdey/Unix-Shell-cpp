#pragma once
#include <sys/types.h>
#include <string>
#include <vector>

struct Job {
  int number;
  pid_t pid;
  std::string commandLine;
  bool running; // true = Running, false = Done
};

// Registers a new background job, returns its assigned job number.
int registerBackgroundJob(pid_t pid, const std::string& commandLine);

// Returns all currently tracked jobs, in registration order
const std::vector<Job>& allJobs();

