#include "seadsa/support/Stats.hh"

#include "seadsa/config.h"

#include "llvm/Support/raw_ostream.h"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace seadsa {

bool SeaDsaStatsFlag = false;
static unsigned SeaDsaStatsActiveAnalyses = 0;
void SeaDsaEnableStats(bool v) { SeaDsaStatsFlag = v; }

void SeaDsaStatsBeginAnalysis() {
  if (!SeaDsaStatsFlag) {
    return;
  }

  if (SeaDsaStatsActiveAnalyses == 0) {
    SeaDsaStats::reset();
  }
  ++SeaDsaStatsActiveAnalyses;
}

void SeaDsaStatsEndAnalysis(llvm::raw_ostream &out) {
  if (!SeaDsaStatsFlag || SeaDsaStatsActiveAnalyses == 0) {
    return;
  }

  --SeaDsaStatsActiveAnalyses;
  if (SeaDsaStatsActiveAnalyses == 0) {
    SeaDsaStats::Print(out);
  }
}

#ifndef SEADSA_STATS

void SeaDsaStats::reset() {}

void SeaDsaStats::count(const std::string &name) { (void)name; }

void SeaDsaStats::start(const std::string &name) { (void)name; }

void SeaDsaStats::stop(const std::string &name) { (void)name; }

void SeaDsaStats::resume(const std::string &name) { (void)name; }

void SeaDsaStats::Print(llvm::raw_ostream &o) {
  o << "\n\n************** STATS ***************** \n";
  o << "SeaDsa compiled without support for scoped stats "
       "(SEADSA_SCOPED_STATS). "
       "Compile SeaDsa with -DSEADSA_STATS=ON\n";
  o << "************** STATS END ***************** \n";
}

ScopedSeaDsaStats::ScopedSeaDsaStats(const char *name, bool use_count)
    : m_name(name), m_active(false) {
  (void)use_count;
}

ScopedSeaDsaStats::~ScopedSeaDsaStats() {}

#else

namespace {

using Clock = std::chrono::steady_clock;

struct Stopwatch {
  Clock::time_point started = Clock::time_point();
  Clock::time_point finished = Clock::time_point();
  std::chrono::nanoseconds elapsed = std::chrono::nanoseconds::zero();

  void start() {
    started = Clock::now();
    finished = Clock::time_point();
    elapsed = std::chrono::nanoseconds::zero();
  }

  void resume() {
    if (finished != Clock::time_point()) {
      elapsed += std::chrono::duration_cast<std::chrono::nanoseconds>(
          finished - started);
      started = Clock::now();
      finished = Clock::time_point();
    } else if (started == Clock::time_point()) {
      started = Clock::now();
    }
  }

  void stop() {
    if (started != Clock::time_point() && finished == Clock::time_point()) {
      finished = Clock::now();
    }
  }

  std::chrono::nanoseconds total() const {
    if (started == Clock::time_point()) {
      return elapsed;
    }
    if (finished == Clock::time_point()) {
      return elapsed + std::chrono::duration_cast<std::chrono::nanoseconds>(
                           Clock::now() - started);
    }
    return elapsed + std::chrono::duration_cast<std::chrono::nanoseconds>(
                         finished - started);
  }
};

std::unordered_map<std::string, unsigned> &getCounters() {
  static std::unordered_map<std::string, unsigned> counters;
  return counters;
}

std::unordered_map<std::string, Stopwatch> &getTimers() {
  static std::unordered_map<std::string, Stopwatch> timers;
  return timers;
}

std::string formatElapsedTime(std::chrono::nanoseconds elapsed) {
  using namespace std::chrono;

  const auto ns = elapsed.count();
  std::ostringstream os;

  if (ns >= 1000000000LL) {
    const double secs = duration<double>(elapsed).count();
    os << std::fixed << std::setprecision(2) << secs << " s";
  } else if (ns >= 1000000LL) {
    const double ms = duration<double, std::milli>(elapsed).count();
    os << std::fixed << std::setprecision(2) << ms << " ms";
  } else if (ns >= 1000LL) {
    const double us = duration<double, std::micro>(elapsed).count();
    os << std::fixed << std::setprecision(0) << us << " us";
  } else {
    os << ns << " ns";
  }

  return os.str();
}

} // namespace

void SeaDsaStats::reset() {
  if (SeaDsaStatsFlag) {
    getCounters().clear();
    getTimers().clear();
  }
}

void SeaDsaStats::count(const std::string &name) {
  if (SeaDsaStatsFlag) {
    ++getCounters()[name];
  }
}

void SeaDsaStats::start(const std::string &name) {
  if (SeaDsaStatsFlag) {
    getTimers()[name].start();
  }
}

void SeaDsaStats::stop(const std::string &name) {
  if (SeaDsaStatsFlag) {
    getTimers()[name].stop();
  }
}

void SeaDsaStats::resume(const std::string &name) {
  if (SeaDsaStatsFlag) {
    getTimers()[name].resume();
  }
}

void SeaDsaStats::Print(llvm::raw_ostream &o) {
  if (!SeaDsaStatsFlag) {
    o << "\n\n************** STATS ***************** \n";
    o << "Need to call SeaDsaEnableStats()\n";
    o << "************** STATS END ***************** \n";
    return;
  }

  struct StatsRow {
    std::string name;
    unsigned count = 0;
    std::string time;
  };

  std::vector<StatsRow> rows;
  rows.reserve(std::max(getCounters().size(), getTimers().size()));

  std::unordered_map<std::string, std::size_t> row_index;
  row_index.reserve(getCounters().size() + getTimers().size());

  for (const auto &kv : getCounters()) {
    auto it = row_index.find(kv.first);
    if (it == row_index.end()) {
      row_index.emplace(kv.first, rows.size());
      rows.push_back({kv.first, kv.second, "-"});
    } else {
      rows[it->second].count = kv.second;
    }
  }

  for (const auto &kv : getTimers()) {
    const auto total = kv.second.total();
    const std::string pretty_time = formatElapsedTime(total);

    auto it = row_index.find(kv.first);
    if (it == row_index.end()) {
      row_index.emplace(kv.first, rows.size());
      rows.push_back({kv.first, 0, pretty_time});
    } else {
      rows[it->second].time = pretty_time;
    }
  }

  std::sort(rows.begin(), rows.end(),
            [](const StatsRow &a, const StatsRow &b) {
              return a.name < b.name;
            });

  const std::string function_header = "function";
  const std::string called_header = "called";
  const std::string time_header = "time";

  std::size_t function_width = function_header.size();
  std::size_t called_width = called_header.size();
  std::size_t time_width = time_header.size();

  for (const auto &row : rows) {
    function_width = std::max(function_width, row.name.size());
    const auto count_width = std::to_string(row.count).size();
    called_width = std::max(called_width, count_width);
    time_width = std::max(time_width, row.time.size());
  }

  const auto printSeparator = [&](char ch) {
    o << "+" << std::string(function_width + 2, ch) << "+"
      << std::string(called_width + 2, ch) << "+"
      << std::string(time_width + 2, ch) << "+\n";
  };

  const auto printCell = [&](const std::string &text, std::size_t width,
                             bool right_align = false) {
    o << " ";
    const std::size_t pad = (width > text.size()) ? (width - text.size()) : 0;
    if (right_align) {
      o << std::string(pad, ' ') << text;
    } else {
      o << text << std::string(pad, ' ');
    }
    o << " ";
  };

  o << "\n\n************** STATS ***************** \n";
  printSeparator('-');
  o << "|";
  printCell(function_header, function_width);
  o << "|";
  printCell(called_header, called_width, true);
  o << "|";
  printCell(time_header, time_width, true);
  o << "|\n";
  printSeparator('=');

  for (const auto &row : rows) {
    o << "|";
    printCell(row.name, function_width);
    o << "|";
    printCell(std::to_string(row.count), called_width, true);
    o << "|";
    printCell(row.time, time_width, true);
    o << "|\n";
  }

  printSeparator('-');
  o << "************** STATS END ***************** \n";
}

ScopedSeaDsaStats::ScopedSeaDsaStats(const char *name, bool use_count)
    : m_name(name), m_active(SeaDsaStatsFlag) {
  if (!m_active) {
    return;
  }
  SeaDsaStats::resume(m_name);
  if (use_count) {
    SeaDsaStats::count(m_name);
  }
}

ScopedSeaDsaStats::~ScopedSeaDsaStats() {
  if (m_active) {
    SeaDsaStats::stop(m_name);
  }
}

#endif

} // namespace seadsa
