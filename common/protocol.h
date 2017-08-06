#ifndef COMMON_PROTOCOL_H_
#define COMMON_PROTOCOL_H_

#include <stdio.h>

#include <memory>

#include "base/values.h"

namespace base {
  class TimeTicks;
  class TimeDelta;
}

namespace common {

std::unique_ptr<base::Value> ReadMessage(FILE* fp,
                                         const base::TimeDelta& timeout,
                                         const base::TimeTicks& start_time);
void WriteMessage(FILE* fp, const base::Value& value);

}  // namespace common

#endif  // COMMON_PROTOCOL_H_
