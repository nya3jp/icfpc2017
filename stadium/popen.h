#ifndef STADIUM_POPEN_H_
#define STADIUM_POPEN_H_

#include <stdio.h>
#include <unistd.h>

#include <string>

#include "base/macros.h"
#include "base/files/scoped_file.h"

namespace stadium {

class Popen {
 public:
  explicit Popen(const std::string& shell);
  ~Popen();

  FILE* stdin_write() const { return stdin_write_.get(); }
  FILE* stdout_read() const { return stdout_read_.get(); }

  void Wait();

 private:
  pid_t pid_;
  base::ScopedFILE stdin_write_;
  base::ScopedFILE stdout_read_;

  DISALLOW_COPY_AND_ASSIGN(Popen);
};

}  // namespace stadium

#endif  // STADIUM_POPEN_H_
