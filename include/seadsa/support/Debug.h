// SPDX-FileCopyrightText: 2026 The sea-dsa Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once
#include <set>
#include <string>

// debug-log
#define DOG(CODE) LOG(DEBUG_TYPE, CODE)
// named-log
#define LOG(TAG,CODE) \
  do {                                                                         \
    if (::seadsa::SeaDsaLog.count(TAG) > 0) {                                  \
      CODE;                                                                    \
    } else {                                                                   \
      ;                                                                        \
    }                                                                          \
  } while (false)


namespace seadsa {

  extern std::set<std::string> SeaDsaLog;
  void SeaDsaEnableLog (std::string x);

  struct SeaDsaLogOpt {
  void operator=(const std::string &tag) const { SeaDsaEnableLog(tag); }
  };
} // namespace seadsa

