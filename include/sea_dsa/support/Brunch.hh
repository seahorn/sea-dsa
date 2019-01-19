#pragma once

#include "llvm/ADT/Twine.h"
#include "llvm/Support/raw_ostream.h"

#include "sea_dsa/support/Debug.h"

#include <sys/resource.h>

#include <cassert>
#include <memory>

/// Brunch stats can be enabled by passing --log=brunch to a sea-dsa binary.
/// Brunch progress can be enabled by passing --log=brunch to a sea-dsa binary.

#define SEA_DSA_BRUNCH_STAT(...)                                               \
  LOG("brunch", sea_dsa::PrintBrunchStat(__VA_ARGS__))
#define SEA_DSA_BRUNCH_PROGRESS(...)                                           \
  LOG("brunch-progress", sea_dsa::PrintBrunchProgress(__VA_ARGS__))

namespace sea_dsa {
template <typename T> void PrintBrunchStat(llvm::Twine key, const T &value) {
  llvm::errs() << "BRUNCH_STAT " << key << " " << value << "\n";
}

template <typename T>
void PrintBrunchProgress(llvm::Twine key, const T &value, const T &max = "") {
  llvm::errs() << "BRUNCH_PROGRESS " << key << " " << value << " / " << max
               << "\n";
}

long GetCurrentMemoryUsageKb();

class BrunchTimer {
public:
  BrunchTimer(llvm::StringRef name)
      : m_name(name), m_startTime(getCurrentTimeMsSteady()) {}

  void stop() {
    pause();
    SEA_DSA_BRUNCH_STAT(llvm::Twine("SEA_TIME_", m_name) + "_MS", m_elapsed);
    m_stopped = true;
  }

  void pause() {
    if (m_paused)
      return;

    const long endTime = getCurrentTimeMsSteady();
    m_elapsed += endTime - m_startTime;
    m_paused = true;
  }

  void resume() {
    assert(m_paused);
    m_startTime = getCurrentTimeMsSteady();
    m_paused = false;
  }

  ~BrunchTimer() { assert(m_stopped); }

private:
  static long getCurrentTimeMsSteady();

  const llvm::StringRef m_name;
  long m_startTime;
  long m_elapsed = 0;
  bool m_stopped = false;
  bool m_paused = false;
};

} // namespace sea_dsa