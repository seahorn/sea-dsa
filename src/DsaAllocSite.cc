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

  /// XXX DSAllocSite is unique and global to a Graph. It does not
  /// XXX make sense  to have a method to copy call-paths from one
  /// XXX DSAllocSite to another -- there is only one and its unique.
  ///
  /// XXX Instead, have a method 'portPaths' that takes a DSAllocSite from
  /// XXX a different graph and a CallEdge, checks that the call-edge is correct
  /// XXX copies all the paths extending them with a call-edge before copying.
void DSAllocSite::copyPaths(const DSAllocSite &other) {
  // XXX This should be an assertion, not silent ignore
  if (this == &other)
    return;

  /// XXX if not tracked then callPaths are empty, nothing to copy
  /// XXX at least have an assertion to check for that
  if (m_allocSite->getName() != TrackAllocSite && !TrackAllAllocSites)
    return;

  for (auto &x : other.m_callPaths)
    assert(!x.empty());

  for (auto &x : m_callPaths)
    assert(!x.empty());

  m_callPaths.reserve(m_callPaths.size() + other.m_callPaths.size());
  m_callPaths.insert(m_callPaths.end(), other.m_callPaths.begin(),
                     other.m_callPaths.end());
  std::sort(m_callPaths.begin(), m_callPaths.end());
  m_callPaths.erase(std::unique(m_callPaths.begin(), m_callPaths.end()),
                    m_callPaths.end());

  for (auto &x : m_callPaths)
    assert(!x.empty());

  static size_t cnt = 1;
  if (m_callPaths.size() > cnt &&
      (!TrackAllocSite.empty() || TrackAllAllocSites)) {
    llvm::errs() << "[DSA] Info: Num call paths: " << m_callPaths.size()
                 << "\n";
    cnt *= 10;
  }
}

void DSAllocSite::setLocalStep(const llvm::Function *f) {
  assert(m_callPaths.empty());
  if (m_allocSite->getName() != TrackAllocSite && !TrackAllAllocSites)
    return;

  m_callPaths.push_back({});
  m_callPaths.back().push_back({Local, f});
}

void DSAllocSite::addStep(StepKind kind, llvm::ImmutableCallSite cs) {
  assert(kind != Local);
  if (m_allocSite->getName() != TrackAllocSite && !TrackAllAllocSites)
    return;

  DsaCallSite dsaCS(cs);
  const llvm::Function *pred = dsaCS.getCallee();
  const llvm::Function *succ = dsaCS.getCaller();
  if (kind == TopDown)
    std::swap(pred, succ);
  Step s(kind, succ);

  if (m_callPaths.empty())
    assert(false); // setLocalStep(pred);


  /// XXX Too complex. A path is extended only when an allocation site is
  /// XXX copied from one graph to another. Extend the path during copying
  /// XXX ensure that every copy is due to an appropriate call-site.
  /// XXX Don't break this into more complex than it has to be.
  /// XXX This also assumes that every function appears only once on a call-path
  /// XXX while this might be true right now, the assumption is not justified.
  for (auto &Path : m_callPaths) {
    assert(!Path.empty() && "Local step missing");
    assert(Path.front().first == Local && "First step must be Local");

    if (Path.back().second != pred)
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

}
