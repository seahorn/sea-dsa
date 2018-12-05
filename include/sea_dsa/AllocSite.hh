#pragma once

#include "llvm/IR/Value.h"
#include "llvm/Support/raw_ostream.h"

#include <vector>

namespace sea_dsa {

class Graph;

class DSAllocSite {
  llvm::Value *m_allocSite = nullptr;
  enum StepKind { Local, BottomUp, TopDown };
  using Step = std::pair<StepKind, llvm::Function *>;
  std::vector<std::vector<Step>> m_callPaths;
  sea_dsa::Graph *m_owner;

public:
  llvm::Value &getAllocSite() const {
    assert(m_allocSite);
    return *m_allocSite;
  }

  Graph *getOwner() const {
    assert(m_owner);
    return m_owner;
  }

  void addStep(const Step& s) {
    for (auto &Path : m_callPaths)
      Path.push_back(s);
  }

  void copyPaths(const DSAllocSite& other) {
    m_callPaths.insert(m_callPaths.end(), other.m_callPaths.begin(),
                       other.m_callPaths.end());
  }

  void print(llvm::raw_ostream &os = llvm::errs()) const;

  friend llvm::raw_ostream &operator<<(llvm::raw_ostream& os,
                                       const DSAllocSite& AS) {
    AS.print(os);
    return os;
  }

  bool operator<(const DSAllocSite &other) const {
    return m_allocSite < other.m_allocSite;
  }

  bool operator==(const DSAllocSite &other) const {
    return m_allocSite == other.m_allocSite;
  }

private:
  friend sea_dsa::Graph;

  /// Can only be created by Graph. Every DSAllocSite belongs to one Graph.
  DSAllocSite(sea_dsa::Graph &g, llvm::Value& v)
      : m_allocSite(&v), m_owner(&g) {}
  DSAllocSite(sea_dsa::Graph &g, const llvm::Value& v)
      : DSAllocSite(g, const_cast<llvm::Value&>(v)) {}
};

}