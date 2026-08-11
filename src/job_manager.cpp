#include "shell/job_manager.h"

#include <sys/wait.h>
#include <algorithm>

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

void reapFinishedJobs() {
  for (auto& job : jobTable()) {
    if (!job.running) continue; // already known to be done

    int status;
    pid_t result = waitpid(job.pid, &status, WNOHANG);

    if (result == job.pid && WIFEXITED(status)) {
      job.running = false;
    }
    // result == 0 means still running; negative means error
    // (e.g. already reaped elsewhere) — leave as-is for this stage,
    // since only normal exits are in scope.
  }
}

void removeFinishedJobs() {
  auto& jobs = jobTable();
  jobs.erase(
    std::remove_if(jobs.begin(), jobs.end(),
                    [](const Job& j) { return !j.running; }),
    jobs.end()
  );
}
