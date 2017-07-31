// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_LOGGING_H_
#define BASE_LOGGING_H_

#include "glog/logging.h"

#if defined(NDEBUG)
#define DCHECK_IS_ON() 0
#else
#define DCHECK_IS_ON() 1
#endif

#define VPLOG(verboselevel) PLOG_IF(INFO, VLOG_IS_ON(verboselevel))

#if DCHECK_IS_ON()
#define DPLOG(severity) PLOG(severity)
#define DVPLOG(verboselevel) PLOG_IF(INFO, VLOG_IS_ON(verboselevel))
#else
#define DPLOG(severity) CHECK(true)
#define DVPLOG(verboselevel) CHECK(true)
#endif

// RAW_LOG is not supported.
#define RAW_LOG(level, message) CHECK(true)

#define NOTREACHED() DCHECK(false)

#endif  // BASE_LOGGING_H_
