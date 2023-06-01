/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef INCLUDE_ARC_COMMON_H_
#define INCLUDE_ARC_COMMON_H_

#include <android-base/logging.h>
#include <string>

using ::android::base::DEBUG;
using ::android::base::ERROR;
using ::android::base::FATAL;
using ::android::base::FATAL_WITHOUT_ABORT;
using ::android::base::INFO;
using ::android::base::VERBOSE;
using ::android::base::WARNING;

#define LOGF(level) LOG(level) << __FUNCTION__ << "(): "
#define LOGFID(level, id) LOG(level) << __FUNCTION__ << "(): id: " << id << ": "

#define VLOGF(level) VLOG(level) << __FUNCTION__ << "(): "
#define VLOGFID(level, id)                                                     \
    VLOG(level) << __FUNCTION__ << "(): id: " << id << ": "

#define VLOGF_ENTER() LOGF(DEBUG) << "enter"
#define VLOGF_EXIT() LOGF(DEBUG) << "exit"

inline std::string FormatToString(int32_t format) {
    return std::string(reinterpret_cast<char *>(&format), 4);
}

#endif // INCLUDE_ARC_COMMON_H_
