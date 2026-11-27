#pragma once

#include "seadsa/config.h"

#include "llvm/Support/raw_ostream.h"

#include <string>

namespace seadsa {

/// Runtime toggle. Statistics are collected only when the library is
/// compiled with -DSEADSA_STATS=ON *and* this flag is set.
extern bool SeaDsaStatsFlag;
void SeaDsaEnableStats(bool v = true);

/// Nested analysis sessions: counters are reset when the outermost
/// session begins and printed when the outermost session ends.
void SeaDsaStatsBeginAnalysis();
void SeaDsaStatsEndAnalysis(llvm::raw_ostream &out = llvm::errs());

class SeaDsaStats {
public:
  static void reset();
  static void count(const std::string &name);
  static void start(const std::string &name);
  static void stop(const std::string &name);
  static void resume(const std::string &name);
  static void Print(llvm::raw_ostream &out);
};

/// Times (and optionally counts) the enclosing scope under \p name.
/// \p name must outlive the scope (use a string literal). Cheap when
/// stats are disabled at runtime: no allocation is performed until
/// the flag is on.
class ScopedSeaDsaStats {
  const char *m_name;
  bool m_active;

public:
  explicit ScopedSeaDsaStats(const char *name, bool use_count = true);
  ~ScopedSeaDsaStats();
};

} // namespace seadsa

#define SEADSA_STATS_JOIN_IMPL(a, b) a##b
#define SEADSA_STATS_JOIN(a, b) SEADSA_STATS_JOIN_IMPL(a, b)

#ifdef SEADSA_STATS
#define SEADSA_SCOPED_STATS(name, active) SEADSA_SCOPED_STATS_(name, active)
#define SEADSA_SCOPED_STATS_(name, active) SEADSA_SCOPED_STATS_##active(name)
#define SEADSA_SCOPED_STATS_0(name)
#define SEADSA_SCOPED_STATS_1(name)                                            \
  seadsa::ScopedSeaDsaStats SEADSA_STATS_JOIN(__seadsa_scoped_stats_,          \
                                              __LINE__)(name)

#define SEADSA_COUNT_STATS(name, active) SEADSA_COUNT_STATS_(name, active)
#define SEADSA_COUNT_STATS_(name, active) SEADSA_COUNT_STATS_##active(name)
#define SEADSA_COUNT_STATS_0(name)
#define SEADSA_COUNT_STATS_1(name) seadsa::SeaDsaStats::count(name)

#define SEADSA_SCOPED_TIMER_STATS(name, active)                                \
  SEADSA_SCOPED_TIMER_STATS_(name, active)
#define SEADSA_SCOPED_TIMER_STATS_(name, active)                               \
  SEADSA_SCOPED_TIMER_STATS_##active(name)
#define SEADSA_SCOPED_TIMER_STATS_0(name)
#define SEADSA_SCOPED_TIMER_STATS_1(name)                                      \
  seadsa::ScopedSeaDsaStats SEADSA_STATS_JOIN(__seadsa_scoped_stats_,          \
                                              __LINE__)(name, false)
#else
#define SEADSA_SCOPED_STATS(name, active)
#define SEADSA_COUNT_STATS(name, active)
#define SEADSA_SCOPED_TIMER_STATS(name, active)
#endif
