// SPDX-FileCopyrightText: 2026 The sea-dsa Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <seadsa/support/Debug.h>

std::set<std::string> seadsa::SeaDsaLog;

void seadsa::SeaDsaEnableLog (std::string x) {
  if (x.empty()) return;
  seadsa::SeaDsaLog.insert (x); 
}





