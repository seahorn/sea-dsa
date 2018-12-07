#pragma once

#include "llvm/IR/CallSite.h"
#include "llvm/IR/Value.h"
#include "llvm/Support/raw_ostream.h"

#include <vector>

namespace sea_dsa {

class Graph;

class DSAllocSite {
public:
  enum StepKind { Local, BottomUp, TopDown };
  using Step = std::pair<StepKind, const llvm::Function *>;
  using CallEdge = std::pair<StepKind, llvm::ImmutableCallSite>;
  using CallPath = std::vector<Step>;
  using CallPaths = std::vector<CallPath>;

private:
  /// XXX why not const?
  //// why not reference?
  llvm::Value *m_allocSite;
  /// XXX why not cost?
  /// XXX why not reference
  sea_dsa::Graph *m_owner;
  CallPaths m_callPaths;

public:
  llvm::Value &getAllocSite() const {
    assert(m_allocSite);
    return *m_allocSite;
  }

  /// XXX why not reference if cannot be null?
  Graph *getOwner() const {
    assert(m_owner);
    return m_owner;
  }

  const CallPaths& getPaths() const {return m_callPaths;}

  void setLocalStep(const llvm::Function *f);
  void addStep(StepKind kind, llvm::ImmutableCallSite cs);
  void copyPaths(const DSAllocSite& other);

  bool hasCallPaths() const { return !m_callPaths.empty(); }

  void print(llvm::raw_ostream &os = llvm::errs()) const;
  void printCallPaths(llvm::raw_ostream &os = llvm::errs()) const;

  friend llvm::raw_ostream &operator<<(llvm::raw_ostream& os,
                                       const DSAllocSite& AS) {
    AS.print(os);
    return os;
  }

  bool operator<(const DSAllocSite &other) const {
    assert(m_owner == other.m_owner);
    return m_allocSite < other.m_allocSite;
  }

  bool operator==(const DSAllocSite &other) const {
    assert(m_owner == other.m_owner);
    return m_allocSite == other.m_allocSite;
  }

  bool operator!=(const DSAllocSite &other) const {
    return !(*this == other);
  }

  bool operator>(const DSAllocSite &other) const {
    return (other != *this) && !(other < *this);
  }

private:
  friend sea_dsa::Graph;

  /// Can only be created by Graph. Every DSAllocSite belongs to one Graph.
  /// XXX why is Value not const?
  DSAllocSite(sea_dsa::Graph &g, llvm::Value &v)
      : m_allocSite(&v), m_owner(&g) {}
  DSAllocSite(sea_dsa::Graph &g, const llvm::Value &v)
      : DSAllocSite(g, const_cast<llvm::Value &>(v)) {}
};

}
