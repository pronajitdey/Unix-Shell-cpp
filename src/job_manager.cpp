#include "shell/job_manager.h"

static std::vector<Job>& jobTable() {
  static std::vector<Job> jobs;
  return jobs;
}

static int nextJobNumber = 1;

int registerBackgroundJob(pid_t pid, const std::string& commandLine) {
  int number = nextJobNumber++;
  jobTable().push_back(Job{number, pid, commandLine, true});
  return number;
}

const std::vector<Job>& allJobs() {
  return jobTable();
}
