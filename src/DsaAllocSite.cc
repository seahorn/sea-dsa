#include "sea_dsa/AllocSite.hh"

#include "llvm/IR/Function.h"

static llvm::cl::opt<std::string>
    TrackAllocSite("sea-dsa-track-alloc-site",
                   llvm::cl::desc("DSA: Track allocation site"),
                   llvm::cl::init(""), llvm::cl::Hidden);

namespace sea_dsa {

void DSAllocSite::copyPaths(const DSAllocSite &other) {
  if (m_allocSite->getName() != TrackAllocSite)
    return;

  m_callPaths.insert(m_callPaths.end(), other.m_callPaths.begin(),
                     other.m_callPaths.end());
  static size_t cnt = 1;
  if (m_callPaths.size() > cnt) {
    llvm::errs() << "Size: " << m_callPaths.size() << "\n";
    cnt *= 10;
    // llvm::errs() << *this << "\n";
  }
  //  if (cnt > 100) assert(0);

  std::sort(m_callPaths.begin(), m_callPaths.end());
  m_callPaths.erase(std::unique(m_callPaths.begin(), m_callPaths.end()),
                    m_callPaths.end());
  m_callPaths.erase(
      std::remove_if(m_callPaths.begin(), m_callPaths.end(),
                     [](const std::vector<Step> &P) { return P.empty(); }),
      m_callPaths.end());
}

void DSAllocSite::addStep(const Step &s) {
  if (m_allocSite->getName() != TrackAllocSite)
    return;

  if (m_callPaths.empty())
    m_callPaths.push_back({});

  for (auto &Path : m_callPaths) {
    if (!Path.empty() && Path.front().first == Local && s.first == Local) {
      llvm_unreachable("Two local steps!");
    }
    if (!Path.empty() && Path.back() == s)
      continue;

    Path.push_back(s);
  }
}

void DSAllocSite::print(llvm::raw_ostream &os) const {
  using namespace llvm;
  if (m_allocSite) {
    if (isa<Function>(m_allocSite) || isa<BasicBlock>(m_allocSite)) {
      assert(m_allocSite->hasName());
      os << m_allocSite->getName();
    } else {
      m_allocSite->print(os);
    }
  } else
    os << "<nullptr>";

  printCallPaths(os);
}

void DSAllocSite::printCallPaths(llvm::raw_ostream &os) const {
  for (const auto& Path : m_callPaths) {
    os << "\n\t\t(";
    for (const Step &step : Path) {
      if (step.first == Local) {
        os << "LOCAL ";
      } else if (step.first == BottomUp) {
        os << " -BU-> ";
      } else if (step.first == TopDown) {
        os << " <-TD- ";
      }
      if (step.second)
        os << step.second->getName() << " ";
      else
        os << "<nullptr> ";
    }
    os << ")\n";
  }
}

}