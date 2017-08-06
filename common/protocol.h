#ifndef COMMON_PROTOCOL_H_
#define COMMON_PROTOCOL_H_

#include <stdio.h>

#include <memory>

#include "base/optional.h"
#include "base/values.h"

namespace base {
  class TimeTicks;
  class TimeDelta;
}

namespace common {

std::unique_ptr<base::Value> ReadMessage(FILE* fp,
                                         const base::TimeDelta& timeout,
                                         const base::TimeTicks& start_time);

// Recieve a message without timeout.
std::unique_ptr<base::Value> ReadMessage(FILE* fp);

void WriteMessage(FILE* fp, const base::Value& value);

void WritePing(FILE* fp, const std::string& name);
base::Optional<std::string> ReadPing(FILE* fp);

void WritePong(FILE* fp, const std::string& name);
base::Optional<std::string> ReadPong(FILE* fp);

}  // namespace common

#endif  // COMMON_PROTOCOL_H_
