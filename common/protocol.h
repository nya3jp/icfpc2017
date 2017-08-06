#ifndef COMMON_PROTOCOL_H_
#define COMMON_PROTOCOL_H_

#include <stdio.h>

#include <memory>

#include "base/values.h"

namespace common {

std::unique_ptr<base::Value> ReadMessage(FILE* fp);
void WriteMessage(FILE* fp, const base::Value& value);

}  // namespace common

#endif  // COMMON_PROTOCOL_H_
