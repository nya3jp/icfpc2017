#include "stadium/popen.h"

#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <utility>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"

namespace stadium {

namespace {

std::pair<base::ScopedFD, base::ScopedFD> CreatePipe(int flags = 0) {
  int fds[2];
  PCHECK(pipe2(fds, flags) == 0);
  return {base::ScopedFD(fds[0]), base::ScopedFD(fds[1])};
}

}  // namespace

Popen::Popen(const std::string& shell) {
  base::ScopedFD stdin_read, stdin_write;
  std::tie(stdin_read, stdin_write) = CreatePipe();
  base::ScopedFD stdout_read, stdout_write;
  std::tie(stdout_read, stdout_write) = CreatePipe();

  pid_t pid = fork();
  PCHECK(pid >= 0);
  if (pid == 0) {
    int max_fd = sysconf(_SC_OPEN_MAX);

    PCHECK(HANDLE_EINTR(dup2(stdin_read.get(), 0)) >= 0);
    stdin_read.reset();
    stdin_write.reset();

    PCHECK(HANDLE_EINTR(dup2(stdout_write.get(), 1)) >= 0);
    stdout_read.reset();
    stdout_write.reset();

    // Close all FDs other than stdin, stdout, stderr.
    for (int i = 3; i < max_fd; ++i)
      close(i);

    // Child.
    char* const commands [] = {
      const_cast<char*>("/bin/bash"),
      const_cast<char*>("-c"),
      const_cast<char*>(shell.c_str()),
      nullptr,
    };

    execv(commands[0], commands);
    PCHECK(false) << "EXEC is failed.";
  }

  // Parent
  stdin_read.reset();
  stdout_write.reset();

  pid_ = pid;
  stdin_write_.reset(fdopen(stdin_write.release(), "w"));
  CHECK(stdin_write_);
  stdout_read_.reset(fdopen(stdout_read.release(), "r"));
  CHECK(stdout_read_);
}

Popen::~Popen() { Wait(); }

void Popen::Wait() {
  // Maybe we need to kill all decendants?
  kill(pid_, SIGKILL);
  int status = -1;
  CHECK_EQ(pid_, waitpid(pid_, &status, 0));
}

}  // namespace stadium
