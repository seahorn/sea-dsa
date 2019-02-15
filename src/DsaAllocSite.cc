#include "sea_dsa/AllocSite.hh"

#include "llvm/IR/Function.h"

#include "sea_dsa/CallSite.hh"

#include <algorithm>

static llvm::cl::opt<std::string> TrackAllocSite(
    "sea-dsa-track-alloc-site",
    llvm::cl::desc("DSA: Track allocation site <function_name,value_name>"),
    llvm::cl::init(""), llvm::cl::Hidden);

namespace sea_dsa {

llvm::StringRef getTrackedASFunction() {
  const size_t commaPos = TrackAllocSite.find(',');
  assert(commaPos != llvm::StringRef::npos && "Wrong format");
  llvm::StringRef functionName(TrackAllocSite.data(), commaPos);
  llvm::errs() << "SEA DSA TRACKED FUNCTION: " << functionName << "\n";
  return functionName;
}

llvm::StringRef getTrackedASName() {
  const size_t commaPos = TrackAllocSite.find(',');
  assert(commaPos != llvm::StringRef::npos && "Wrong format");
  llvm::StringRef valueName(TrackAllocSite.data() + commaPos + 1,
                            TrackAllocSite.size() - commaPos - 1);
  llvm::errs() << "SEA DSA TRACKED VALUE: " << valueName << "\n";
  return valueName;
}

void DsaAllocSite::importCallPaths(const DsaAllocSite &other,
                                   const DsaCallSite &cs, bool bu) {
  // Only copy paths from an allocation site corresponding to the same value.
  assert(&m_value == &other.m_value);

  if (TrackAllocSite.empty())
    return;

  if (!TrackAllocSite.empty()) {
    static const llvm::StringRef trackedFunctionName = getTrackedASFunction();
    static const llvm::StringRef trackedValueName = getTrackedASName();

    if (const auto *I = llvm::dyn_cast<llvm::Instruction>(&m_value)) {
      if (I->getFunction()->getName() != trackedFunctionName)
        return;
    } else if (trackedFunctionName != "any" && trackedFunctionName != "global")
      return;

    if (trackedValueName != "any" && m_value.getName() != trackedValueName)
      return;
  }

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
