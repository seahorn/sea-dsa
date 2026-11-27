#include "doctest.h"

#include "seadsa/support/Stats.hh"

#include "llvm/Support/raw_ostream.h"

#include <string>

TEST_CASE("seadsa.stats.api") {
  // The API must be safe to call regardless of whether the library was
  // compiled with SEADSA_STATS.
  seadsa::SeaDsaEnableStats(true);
  seadsa::SeaDsaStatsBeginAnalysis();
  {
    SEADSA_SCOPED_STATS("units.stats.scope", 1);
    seadsa::SeaDsaStats::count("units.stats.counter");
  }
  std::string out;
  llvm::raw_string_ostream os(out);
  seadsa::SeaDsaStatsEndAnalysis(os);
  os.flush();

#ifdef SEADSA_STATS
  CHECK(out.find("units.stats.scope") != std::string::npos);
  CHECK(out.find("units.stats.counter") != std::string::npos);
#else
  // Stub build: the printer emits a banner explaining how to enable stats.
  CHECK(out.find("SEADSA_SCOPED_STATS") != std::string::npos);
#endif
  seadsa::SeaDsaEnableStats(false);
}

TEST_CASE("seadsa.stats.disabled_at_runtime") {
  // With the runtime flag off, sessions are no-ops and print nothing.
  seadsa::SeaDsaEnableStats(false);
  seadsa::SeaDsaStatsBeginAnalysis();
  {
    SEADSA_SCOPED_STATS("units.stats.unused", 1);
  }
  std::string out;
  llvm::raw_string_ostream os(out);
  seadsa::SeaDsaStatsEndAnalysis(os);
  os.flush();
  CHECK(out.empty());
}
