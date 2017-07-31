#include <stdio.h>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/sequenced_worker_pool.h"
#include "gflags/gflags.h"
#include "glog/logging.h"

void Task(int i) {
  printf("task %d\n", i);
}

int main(int argc, char** argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  google::InstallFailureSignalHandler();

  base::MessageLoop main_loop;

  auto pool = make_scoped_refptr(new base::SequencedWorkerPool(4, "pool"));

  for (int i = 0; i < 10; ++i) {
    pool->PostTask(FROM_HERE, base::BindOnce(&Task, i));
  }

  pool->Shutdown();

  return 0;
}
