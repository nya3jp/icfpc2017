#include <stdio.h>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "gflags/gflags.h"
#include "glog/logging.h"

int Add(int a, int b) {
  return a + b;
}

int CallWith42(base::OnceCallback<int(int)> callback) {
  return std::move(callback).Run(42);
}

int main(int argc, char** argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  google::InstallFailureSignalHandler();

  printf("%d\n", CallWith42(base::Bind(&Add, 1)));
  return 0;
}
