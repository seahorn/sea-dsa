#pragma once

#include "sea_dsa/CallSite.hh"

#include <memory>
#include <unordered_map>
#include <vector>

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
public:
  // XXX: use a vector to have more control about the ordering
  // typedef boost::container::flat_set<const llvm::Instruction*> CallSiteSet;
  typedef std::vector<DsaCallSite> CallSiteSet;
  
private:
  typedef std::shared_ptr<CallSiteSet> CallSiteSetRef;
  typedef std::unordered_map<const llvm::Function *, CallSiteSetRef> IndexMap;

  llvm::CallGraph &m_cg;
  IndexMap m_uses;
  IndexMap m_defs;

  static void insert(DsaCallSite CS, CallSiteSet &s) {
    if (std::find(s.begin(), s.end(), CS) == s.end())
      s.push_back(CS);
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
