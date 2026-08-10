#include "shell/job_manager.h"

static int nextJobNumber = 1;

int registerBackgroundJob(pid_t pid, const std::string& commandLine) {
    (void)commandLine; // not stored yet — used starting the "jobs" listing stage
    return nextJobNumber++;
}
