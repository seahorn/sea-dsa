#pragma once

#include "boost/unordered_map.hpp"

namespace llvm {
class Function;
class Instruction;
class CallGraph;
} // namespace llvm

namespace sea_dsa {
/**
 * A wrapper for LLVM CallGraph to precompute some dependencies
 * between callsites.
 **/
class CallGraphWrapper {

  // XXX: use a vector to have more control about the ordering
  typedef std::vector<const llvm::Instruction *> CallSiteSet;
  // typedef boost::container::flat_set<const llvm::Instruction*> CallSiteSet;
  typedef std::shared_ptr<CallSiteSet> CallSiteSetRef;
  typedef boost::unordered_map<const llvm::Function *, CallSiteSetRef> IndexMap;

  llvm::CallGraph &m_cg;
  IndexMap m_uses;
  IndexMap m_defs;

  static void insert(const llvm::Instruction *I, CallSiteSet &s) {
    if (std::find(s.begin(), s.end(), I) == s.end())
      s.push_back(I);
  }

  template <typename Iter>
  static void insert(Iter it, Iter et, CallSiteSet &s) {
    for (; it != et; ++it)
      insert(*it, s);
  }

public:
  CallGraphWrapper(llvm::CallGraph &cg) : m_cg(cg) {}

  void buildDependencies();

  llvm::CallGraph &getCallGraph() { return m_cg; }

  // Return the set of callsites where fn (or any other function
  // in the same SCC) is the callee
  const CallSiteSet &getUses(const llvm::Function &fn) const {
    auto it = m_uses.find(&fn);
    assert(it != m_uses.end());
    return *(it->second);
  }

  // Return the set of callsites defined inside fn (or in any
  // other function in the same SCC)
  const CallSiteSet &getDefs(const llvm::Function &fn) const {
    auto it = m_defs.find(&fn);
    assert(it != m_defs.end());
    return *(it->second);
  }
};
} // namespace sea_dsa
