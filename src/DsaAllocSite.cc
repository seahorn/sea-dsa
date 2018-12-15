#include "sea_dsa/AllocSite.hh"

#include "llvm/IR/Function.h"

#include "sea_dsa/CallSite.hh"

#include <algorithm>
#include <set>

static llvm::cl::opt<std::string>
    TrackAllocSite("sea-dsa-track-alloc-site",
                   llvm::cl::desc("DSA: Track allocation site"),
                   llvm::cl::init(""), llvm::cl::Hidden);
static llvm::cl::opt<bool>
    TrackAllAllocSites("sea-dsa-track-all-alloc-sites",
                       llvm::cl::desc("DSA: Track all allocation sites"),
                       llvm::cl::init(false), llvm::cl::Hidden);

namespace sea_dsa {

void DsaAllocSite::print(llvm::raw_ostream &os) const {
  using namespace llvm;
  if (isa<Function>(m_value) || isa<BasicBlock>(m_value)) {
    assert(m_value->hasName());
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
