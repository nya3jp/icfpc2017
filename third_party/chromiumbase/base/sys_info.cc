// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sys_info.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/sys_info_internal.h"
#include "base/time/time.h"
#include "build/build_config.h"

namespace base {

#if (!defined(OS_MACOSX) || defined(OS_IOS)) && !defined(OS_ANDROID)
std::string SysInfo::HardwareModelName() {
  return std::string();
}
#endif

// static
base::TimeDelta SysInfo::Uptime() {
  // This code relies on an implementation detail of TimeTicks::Now() - that
  // its return value happens to coincide with the system uptime value in
  // microseconds, on Win/Mac/iOS/Linux/ChromeOS and Android.
  int64_t uptime_in_microseconds = TimeTicks::Now().ToInternalValue();
  return base::TimeDelta::FromMicroseconds(uptime_in_microseconds);
}

}  // namespace base
