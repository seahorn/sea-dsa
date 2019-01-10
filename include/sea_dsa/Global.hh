#ifndef __DSA_GLOBAL_HH_
#define __DSA_GLOBAL_HH_

#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"

#include "sea_dsa/BottomUp.hh"
#include "sea_dsa/CallGraph.hh"
#include "sea_dsa/CallSite.hh"
#include "sea_dsa/Graph.hh"

#include "boost/container/flat_set.hpp"

namespace llvm {
class DataLayout;
class TargetLibraryInfo;
class CallGraph;
} // namespace llvm

namespace sea_dsa {

class AllocWrapInfo;

enum GlobalAnalysisKind { CONTEXT_INSENSITIVE, CONTEXT_SENSITIVE, FLAT_MEMORY };

// Common API for global analyses
class GlobalAnalysis {
protected:
  GlobalAnalysisKind _kind;

public:
  GlobalAnalysis(GlobalAnalysisKind kind) : _kind(kind) {}

  virtual ~GlobalAnalysis() {}

  GlobalAnalysisKind kind() const { return _kind; }

  virtual bool runOnModule(llvm::Module &M) = 0;

  virtual const Graph &getGraph(const llvm::Function &F) const = 0;

  virtual Graph &getGraph(const llvm::Function &F) = 0;

  virtual bool hasGraph(const llvm::Function &F) const = 0;

};

// Context-insensitive dsa analysis
class ContextInsensitiveGlobalAnalysis : public GlobalAnalysis {
public:
  typedef typename Graph::SetFactory SetFactory;

private:
  typedef std::unique_ptr<Graph> GraphRef;

  const llvm::DataLayout &m_dl;
  const llvm::TargetLibraryInfo &m_tli;
  const AllocWrapInfo &m_allocInfo;
  llvm::CallGraph &m_cg;
  SetFactory &m_setFactory;
  GraphRef m_graph;
  // functions represented in m_graph
  boost::container::flat_set<const llvm::Function *> m_fns;

  void resolveArguments(DsaCallSite &cs, Graph &g);

public:
  ContextInsensitiveGlobalAnalysis(const llvm::DataLayout &dl,
                                   const llvm::TargetLibraryInfo &tli,
                                   const AllocWrapInfo &allocInfo,
                                   llvm::CallGraph &cg, SetFactory &setFactory,
                                   const bool useFlatMemory)
      : GlobalAnalysis(useFlatMemory ? FLAT_MEMORY : CONTEXT_INSENSITIVE),
        m_dl(dl), m_tli(tli), m_allocInfo(allocInfo), m_cg(cg),
        m_setFactory(setFactory), m_graph(nullptr) {}

  bool runOnModule(llvm::Module &M) override;

  const Graph &getGraph(const llvm::Function &fn) const override;

  Graph &getGraph(const llvm::Function &fn) override;

  bool hasGraph(const llvm::Function &fn) const override;
};

template <typename T> class WorkList {
public:
  WorkList();
  bool empty() const;
  size_t size() const;
  void enqueue(const T &e);
  const T &dequeue();

private:
  struct impl;
  std::unique_ptr<impl> m_pimpl;
};

// Context-sensitive dsa analysis
class ContextSensitiveGlobalAnalysis : public GlobalAnalysis {
public:
  typedef typename Graph::SetFactory SetFactory;

private:
  typedef std::shared_ptr<Graph> GraphRef;
  typedef BottomUpAnalysis::GraphMap GraphMap;
  enum PropagationKind { DOWN, UP, NONE };

  const llvm::DataLayout &m_dl;
  const llvm::TargetLibraryInfo &m_tli;
  const AllocWrapInfo &m_allocInfo;
  llvm::CallGraph &m_cg;
  SetFactory &m_setFactory;

public:
  GraphMap m_graphs;

  void cloneAndResolveArguments(const DsaCallSite &cs, Graph &callerG,
                                Graph &calleeG);

  PropagationKind decidePropagation(const DsaCallSite &cs, Graph &callerG,
                                    Graph &calleeG);

  void propagateTopDown(const DsaCallSite &cs, Graph &callerG, Graph &calleeG);

  void propagateBottomUp(const DsaCallSite &cs, Graph &calleeG, Graph &callerG);

  bool checkNoMorePropagation();

public:
  ContextSensitiveGlobalAnalysis(const llvm::DataLayout &dl,
                                 const llvm::TargetLibraryInfo &tli,
                                 const AllocWrapInfo &allocInfo,
                                 llvm::CallGraph &cg, SetFactory &setFactory)
      : GlobalAnalysis(CONTEXT_SENSITIVE), m_dl(dl), m_tli(tli),
        m_allocInfo(allocInfo), m_cg(cg), m_setFactory(setFactory) {}

  bool runOnModule(llvm::Module &M) override;

  const Graph &getGraph(const llvm::Function &fn) const override;

  Graph &getGraph(const llvm::Function &fn) override;

  bool hasGraph(const llvm::Function &fn) const override;
};

// Llvm passes

class DsaGlobalPass : public llvm::ModulePass {
protected:
  DsaGlobalPass(char &ID) : llvm::ModulePass(ID) {}

public:
  virtual GlobalAnalysis &getGlobalAnalysis() = 0;
};

class FlatMemoryGlobal : public DsaGlobalPass {

  Graph::SetFactory m_setFactory;
  std::unique_ptr<ContextInsensitiveGlobalAnalysis> m_ga;

public:
  static char ID;

  FlatMemoryGlobal();

  void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;

  bool runOnModule(llvm::Module &M) override;

  llvm::StringRef getPassName() const override {
    return "Flat memory Dsa pass";
  }

  GlobalAnalysis &getGlobalAnalysis() override {
    return *(static_cast<GlobalAnalysis *>(&*m_ga));
  }
};

class ContextInsensitiveGlobal : public DsaGlobalPass {

  Graph::SetFactory m_setFactory;
  std::unique_ptr<ContextInsensitiveGlobalAnalysis> m_ga;

public:
  static char ID;

  ContextInsensitiveGlobal();

  void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;

  bool runOnModule(llvm::Module &M) override;

  llvm::StringRef getPassName() const override {
    return "Context-insensitive Dsa global pass";
  }

  GlobalAnalysis &getGlobalAnalysis() override {
    return *(static_cast<GlobalAnalysis *>(&*m_ga));
  }
};

class ContextSensitiveGlobal : public DsaGlobalPass {
  Graph::SetFactory m_setFactory;
  std::unique_ptr<ContextSensitiveGlobalAnalysis> m_ga;

public:
  static char ID;

  ContextSensitiveGlobal();

  void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;

  bool runOnModule(llvm::Module &M) override;

  llvm::StringRef getPassName() const override {
    return "Context sensitive global DSA pass";
  }

  GlobalAnalysis &getGlobalAnalysis() override {
    return *(static_cast<GlobalAnalysis *>(&*m_ga));
  }
};

// Execute operation Op on each callsite until no more changes
template <class GlobalAnalysis, class Op> class CallGraphClosure {

  GlobalAnalysis &m_ga;
  DsaCallGraph &m_dsaCG;
  WorkList<const llvm::Instruction *> m_w;

  void exec_callsite(const DsaCallSite &cs, Graph &calleeG, Graph &callerG);

public:
  CallGraphClosure(GlobalAnalysis &ga, DsaCallGraph &dsaCG)
      : m_ga(ga), m_dsaCG(dsaCG) {}

  bool runOnModule(llvm::Module &M);
};

// Propagate unique scalar flag across callsites
class UniqueScalar {
  DsaCallGraph &m_dsaCG;
  WorkList<const llvm::Instruction *> &m_w;

public:
  UniqueScalar(DsaCallGraph &dsaCG, WorkList<const llvm::Instruction *> &w)
      : m_dsaCG(dsaCG), m_w(w) {}

  void runOnCallSite(const DsaCallSite &cs, Node &calleeN, Node &callerN);
};

// Propagate allocation sites across callsites
class AllocaSite {
  DsaCallGraph &m_dsaCG;
  WorkList<const llvm::Instruction *> &m_w;

public:
  AllocaSite(DsaCallGraph &dsaCG, WorkList<const llvm::Instruction *> &w)
      : m_dsaCG(dsaCG), m_w(w) {}

  void runOnCallSite(const DsaCallSite &cs, Node &calleeN, Node &callerN);
};
} // namespace sea_dsa
#endif
