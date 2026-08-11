#include "shell/job_manager.h"

#include <sys/wait.h>
#include <algorithm>

static std::vector<Job>& jobTable() {
  static std::vector<Job> jobs;
  return jobs;
}

int registerBackgroundJob(pid_t pid, const std::string& commandLine) {
  auto& jobs = jobTable();

  int nextNumber = 1;
  if (!jobs.empty()) {
    int highest = 0;
    for (const auto& job : jobs) {
      highest = std::max(highest, job.number);
    }
    nextNumber = highest + 1;
  }

  jobs.push_back(Job{nextNumber, pid, commandLine, true});
  return nextNumber;
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
