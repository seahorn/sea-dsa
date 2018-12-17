#include "sea_dsa/AllocSite.hh"

#include "llvm/IR/Function.h"

#include "sea_dsa/CallSite.hh"

#include <algorithm>

static llvm::cl::opt<std::string>
    TrackAllocSite("sea-dsa-track-alloc-site",
                   llvm::cl::desc("DSA: Track allocation site"),
                   llvm::cl::init(""), llvm::cl::Hidden);
static llvm::cl::opt<bool>
    TrackAllAllocSites("sea-dsa-track-all-alloc-sites",
                       llvm::cl::desc("DSA: Track all allocation sites"),
                       llvm::cl::init(false), llvm::cl::Hidden);

namespace sea_dsa {

void DsaAllocSite::importCallPaths(const DsaAllocSite &other,
                                   const DsaCallSite &cs,
                                   bool bu) {
  // Only copy paths from an allocation site corresponding to the same value.
  assert(&m_value == &other.m_value);

  if (!TrackAllAllocSites && m_value.getName() != TrackAllocSite)
    return;

  if (other.getCallPaths().empty()) {
    m_callPaths.emplace_back();
    CallPath &ncp = m_callPaths.back();
    // -- if other had no call paths, then it is local to the function
    // -- we are importing it from
    ncp.emplace_back(Local, bu ? cs.getCallee() : cs.getCaller());
    ncp.emplace_back(bu ? BottomUp : TopDown,
                     bu ? cs.getCaller() : cs.getCallee());
  } else {
    // -- copy all call paths and add last function
    for (const CallPath &cp : other.getCallPaths()) {
      m_callPaths.emplace_back(cp);
      CallPath &ncp = m_callPaths.back();
      ncp.emplace_back(bu ? BottomUp : TopDown,
                       bu ? cs.getCaller() : cs.getCallee());
    }
    // -- unique call paths
    std::sort(m_callPaths.begin(), m_callPaths.end());
    m_callPaths.erase(std::unique(m_callPaths.begin(), m_callPaths.end()),
                      m_callPaths.end());
  }
}
void DsaAllocSite::print(llvm::raw_ostream &os) const {
  using namespace llvm;
  if (isa<Function>(m_value) || isa<BasicBlock>(m_value)) {
    assert(m_value.hasName());
    os << m_value.getName();
  } else {
    m_value.print(os);
  }
  printCallPaths(os);
}

void DsaAllocSite::printCallPaths(llvm::raw_ostream &os) const {
  for (const auto &Path : m_callPaths) {
    os << "\n\t\t(";
    for (const Step &step : Path) {
      if (step.first == Local) {
        os << "LOCAL ";
      } else if (step.first == BottomUp) {
        os << " <-BU- ";
      } else if (step.first == TopDown) {
        os << " -TD-> ";
      }
      if (step.second)
        os << step.second->getName();
      else
        os << "<nullptr>";
    }
    os << ")\n";
  }
}

} // namespace sea_dsa
