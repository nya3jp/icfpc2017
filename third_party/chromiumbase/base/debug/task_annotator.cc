// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/task_annotator.h"

#include <array>

#include "base/debug/alias.h"
#include "base/pending_task.h"
#include "base/tracked_objects.h"

namespace base {
namespace debug {

TaskAnnotator::TaskAnnotator() {
}

TaskAnnotator::~TaskAnnotator() {
}

void TaskAnnotator::DidQueueTask(const char* queue_function,
                                 const PendingTask& pending_task) {
}

void TaskAnnotator::RunTask(const char* queue_function,
                            PendingTask* pending_task) {
  tracked_objects::TaskStopwatch stopwatch;
  stopwatch.Start();

  // Before running the task, store the task backtrace with the chain of
  // PostTasks that resulted in this call and deliberately alias it to ensure
  // it is on the stack if the task crashes. Be careful not to assume that the
  // variable itself will have the expected value when displayed by the
  // optimizer in an optimized build. Look at a memory dump of the stack.
  static constexpr int kStackTaskTraceSnapshotSize =
      std::tuple_size<decltype(pending_task->task_backtrace)>::value + 1;
  std::array<const void*, kStackTaskTraceSnapshotSize> task_backtrace;
  task_backtrace[0] = pending_task->posted_from.program_counter();
  std::copy(pending_task->task_backtrace.begin(),
            pending_task->task_backtrace.end(), task_backtrace.begin() + 1);
  debug::Alias(&task_backtrace);

  std::move(pending_task->task).Run();

  stopwatch.Stop();
  tracked_objects::ThreadData::TallyRunOnNamedThreadIfTracking(*pending_task,
                                                               stopwatch);
}

uint64_t TaskAnnotator::GetTaskTraceID(const PendingTask& task) const {
  return (static_cast<uint64_t>(task.sequence_num) << 32) |
         ((static_cast<uint64_t>(reinterpret_cast<intptr_t>(this)) << 32) >>
          32);
}

}  // namespace debug
}  // namespace base
