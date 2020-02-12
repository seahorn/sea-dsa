#ifndef __DSA_GLOBAL_HH_
#define __DSA_GLOBAL_HH_

#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"

#include "sea_dsa/BottomUp.hh"
#include "sea_dsa/CallGraphWrapper.hh"
#include "sea_dsa/CallSite.hh"
#include "sea_dsa/Graph.hh"
#include "sea_dsa/Mapper.hh"

#include "boost/container/flat_set.hpp"

namespace llvm {
class DataLayout;
class TargetLibraryInfo;
class CallGraph;
} // namespace llvm

namespace sea_dsa {

class AllocWrapInfo;

enum class GlobalAnalysisKind {
  // useful for VC generation
  CONTEXT_INSENSITIVE,
  // useful for VC generation
  CONTEXT_SENSITIVE,
  // bottom-up followed by one top-down pass (not useful for VC generation)
  BUTD_CONTEXT_SENSITIVE,
  // bottom-up pass (useful for VC generation)
  BU,
  // enforce one single node for the whole program
  // used when most pessimistic assumptions must be considered in
  // presence of inttoptr and/or external calls.
  FLAT_MEMORY
};

// Common API for global analyses
class GlobalAnalysis {
protected:
  GlobalAnalysisKind _kind;

public:
  GlobalAnalysis(GlobalAnalysisKind kind): _kind(kind) {}

  virtual ~GlobalAnalysis() {}

  GlobalAnalysisKind kind() const { return _kind; }

  virtual bool runOnModule(llvm::Module &M) = 0;

  // return the points-to graph for F upon completion of the global
  // analysis.
  virtual const Graph &getGraph(const llvm::Function &F) const = 0;
  virtual Graph &getGraph(const llvm::Function &F) = 0;

  // return true if F has its own points-to graph upon completion of
  // the global analysis
  virtual bool hasGraph(const llvm::Function &F) const = 0;

  // return the summary points-to graph for F. A summary graph models
  // all the points-to relationships of F ignoring possible callers to
  // F.
  virtual const Graph &getSummaryGraph(const llvm::Function &F) const = 0;
  virtual Graph &getSummaryGraph(const llvm::Function &F) = 0;

  // return true if F has its own summary points-to graph and
  // m_store_sum_graphs = true.
  virtual bool hasSummaryGraph(const llvm::Function &F) const = 0;
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

public:
  ContextInsensitiveGlobalAnalysis(const llvm::DataLayout &dl,
                                   const llvm::TargetLibraryInfo &tli,
                                   const AllocWrapInfo &allocInfo,
                                   llvm::CallGraph &cg, SetFactory &setFactory,
                                   const bool useFlatMemory)
    : GlobalAnalysis(useFlatMemory ?
		     GlobalAnalysisKind::FLAT_MEMORY :
		     GlobalAnalysisKind::CONTEXT_INSENSITIVE),
      m_dl(dl), m_tli(tli), m_allocInfo(allocInfo), m_cg(cg),
      m_setFactory(setFactory), m_graph(nullptr) {}

  // unify caller/callee nodes within the same graph
  static void resolveArguments(DsaCallSite &cs, Graph &g);

  bool runOnModule(llvm::Module &M) override;

  const Graph &getGraph(const llvm::Function &fn) const override;

  Graph &getGraph(const llvm::Function &fn) override;

  bool hasGraph(const llvm::Function &fn) const override;

  // XXX: the notion of summary graph really makes sense if the
  // analysis is context-sensitive so we return the global analysis.
  const Graph &getSummaryGraph(const llvm::Function &F) const override {
    return getGraph(F);
  }
  
  Graph &getSummaryGraph(const llvm::Function &F) override {
    return getGraph(F);    
  }

  bool hasSummaryGraph(const llvm::Function &F) const override {
    return hasGraph(F);
  }
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

// Context-sensitive dsa analysis as described in the SAS'17 paper. It
// ensures that no two disjoint Dsa nodes modified in a function can
// be aliased at any particular call site.
class ContextSensitiveGlobalAnalysis : public GlobalAnalysis {
public:
  typedef typename Graph::SetFactory SetFactory;
  
private:
  typedef std::shared_ptr<Graph> GraphRef;
  typedef BottomUpAnalysis::GraphMap GraphMap;
  enum PropagationKind { DOWN, UP, NONE };

  typedef std::shared_ptr<SimulationMapper> SimulationMapperRef;
  typedef boost::container::flat_map<DsaCallSite, SimulationMapperRef>
      CalleeCallerMapping;

  const llvm::DataLayout &m_dl;
  const llvm::TargetLibraryInfo &m_tli;
  const AllocWrapInfo &m_allocInfo;
  llvm::CallGraph &m_cg;
  SetFactory &m_setFactory;

  // Context-sensitive graphs 
  GraphMap m_graphs;
  // Bottom-up graphs
  GraphMap m_bu_graphs;
  // Whether to store bottom-up graphs
  bool m_store_bu_graphs;
  
  PropagationKind decidePropagation(const DsaCallSite &cs, Graph &callerG,
                                    Graph &calleeG);

  void propagateTopDown(const DsaCallSite &cs, Graph &callerG, Graph &calleeG);

  void propagateBottomUp(const DsaCallSite &cs, Graph &calleeG, Graph &callerG);

  // sanity checks
  bool checkAllNodesAreMapped(const llvm::Function &callee, Graph &calleeG,
                              const SimulationMapper &sm);
  bool checkNoMorePropagation(llvm::CallGraph &cg);

public:
  ContextSensitiveGlobalAnalysis(const llvm::DataLayout &dl,
                                 const llvm::TargetLibraryInfo &tli,
                                 const AllocWrapInfo &allocInfo,
                                 llvm::CallGraph &cg, SetFactory &setFactory,
				 bool storeSummaryGraphs = false);
  
  bool runOnModule(llvm::Module &M) override;

  const Graph &getGraph(const llvm::Function &fn) const override;

  Graph &getGraph(const llvm::Function &fn) override;

  bool hasGraph(const llvm::Function &fn) const override;

  const Graph &getSummaryGraph(const llvm::Function &F) const override;
  
  Graph &getSummaryGraph(const llvm::Function &F) override;

  bool hasSummaryGraph(const llvm::Function &F) const override;  
};

/**
 * Global analysis that runs one bottom-up followed by one top-down
 * pass.  This is the most precise variant of context-sensitive
 * DSA-style global analysis.
 */
class BottomUpTopDownGlobalAnalysis : public GlobalAnalysis {
public:
  typedef typename Graph::SetFactory SetFactory;

private:
  typedef std::shared_ptr<Graph> GraphRef;
  typedef BottomUpAnalysis::GraphMap GraphMap;

  const llvm::DataLayout &m_dl;
  const llvm::TargetLibraryInfo &m_tli;
  const AllocWrapInfo &m_allocInfo;
  llvm::CallGraph &m_cg;
  SetFactory &m_setFactory;
  // Context-sensitive graphs
  GraphMap m_graphs;
  // Bottom-up graphs
  GraphMap m_bu_graphs;
  // whether to store bottom-up graphs
  bool m_store_bu_graphs;
  
public:
  BottomUpTopDownGlobalAnalysis(const llvm::DataLayout &dl,
                                const llvm::TargetLibraryInfo &tli,
                                const AllocWrapInfo &allocInfo,
                                llvm::CallGraph &cg, SetFactory &setFactory,
				bool storeSummaryGraphs = false);

  bool runOnModule(llvm::Module &M) override;

  const Graph &getGraph(const llvm::Function &fn) const override;

  Graph &getGraph(const llvm::Function &fn) override;

  bool hasGraph(const llvm::Function &fn) const override;

  const Graph &getSummaryGraph(const llvm::Function &F) const override;
  
  Graph &getSummaryGraph(const llvm::Function &F) override;

  bool hasSummaryGraph(const llvm::Function &F) const override;    
};

/**
 * Global analysis that runs only the bottom-up pass.
 */
class BottomUpGlobalAnalysis : public GlobalAnalysis {
public:
  typedef typename Graph::SetFactory SetFactory;

private:
  typedef std::shared_ptr<Graph> GraphRef;
  typedef BottomUpAnalysis::GraphMap GraphMap;

  const llvm::DataLayout &m_dl;
  const llvm::TargetLibraryInfo &m_tli;
  const AllocWrapInfo &m_allocInfo;
  llvm::CallGraph &m_cg;
  SetFactory &m_setFactory;
  GraphMap m_graphs;

public:
  BottomUpGlobalAnalysis(const llvm::DataLayout &dl,
			 const llvm::TargetLibraryInfo &tli,
			 const AllocWrapInfo &allocInfo,
			 llvm::CallGraph &cg, SetFactory &setFactory);

  bool runOnModule(llvm::Module &M) override;

  const Graph &getGraph(const llvm::Function &fn) const override;

  Graph &getGraph(const llvm::Function &fn) override;

  bool hasGraph(const llvm::Function &fn) const override;

  const Graph &getSummaryGraph(const llvm::Function &F) const override {
    return getGraph(F);    
  }
  
  Graph &getSummaryGraph(const llvm::Function &F) override {
    return getGraph(F);
  }

  bool hasSummaryGraph(const llvm::Function &F) const override {
    return hasGraph(F);
  }
};


// Llvm passes

class DsaGlobalPass : public llvm::ModulePass {
protected:
  DsaGlobalPass(char &ID) : llvm::ModulePass(ID) {}

public:
  virtual GlobalAnalysis &getGlobalAnalysis() = 0;
};

// LLVM pass for flat analysis
class FlatMemoryGlobalPass : public DsaGlobalPass {

  Graph::SetFactory m_setFactory;
  std::unique_ptr<ContextInsensitiveGlobalAnalysis> m_ga;

public:
  static char ID;

  FlatMemoryGlobalPass();

  void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;

  bool runOnModule(llvm::Module &M) override;

  llvm::StringRef getPassName() const override {
    return "Flat memory SeaDsa pass";
  }

  GlobalAnalysis &getGlobalAnalysis() override {
    return *(static_cast<GlobalAnalysis *>(&*m_ga));
  }
};

// LLVM pass for context-insensitive analysis
class ContextInsensitiveGlobalPass : public DsaGlobalPass {

  Graph::SetFactory m_setFactory;
  std::unique_ptr<ContextInsensitiveGlobalAnalysis> m_ga;

public:
  static char ID;

  ContextInsensitiveGlobalPass();

  void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;

  bool runOnModule(llvm::Module &M) override;

  llvm::StringRef getPassName() const override {
    return "Context-insensitive SeaDsa global pass";
  }

  GlobalAnalysis &getGlobalAnalysis() override {
    return *(static_cast<GlobalAnalysis *>(&*m_ga));
  }
};

// LLVM pass for SAS'17 context-sensitive analysis
class ContextSensitiveGlobalPass : public DsaGlobalPass {
  Graph::SetFactory m_setFactory;
  std::unique_ptr<ContextSensitiveGlobalAnalysis> m_ga;

public:
  static char ID;

  ContextSensitiveGlobalPass();

  void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;

  bool runOnModule(llvm::Module &M) override;

  llvm::StringRef getPassName() const override {
    return "Context sensitive global SeaDSA pass for VC generation";
  }

  GlobalAnalysis &getGlobalAnalysis() override {
    return *(static_cast<GlobalAnalysis *>(&*m_ga));
  }
};

// LLVM pass for bottom-up + top-down analysis
class BottomUpTopDownGlobalPass : public DsaGlobalPass {
  Graph::SetFactory m_setFactory;
  std::unique_ptr<BottomUpTopDownGlobalAnalysis> m_ga;

public:
  static char ID;

  BottomUpTopDownGlobalPass();

  void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;

  bool runOnModule(llvm::Module &M) override;

  llvm::StringRef getPassName() const override {
    return "Bottom-up + Top-down SeaDsa passes";
  }

  GlobalAnalysis &getGlobalAnalysis() override {
    return *(static_cast<GlobalAnalysis *>(&*m_ga));
  }
};

// LLVM pass for bottom-up analysis
class BottomUpGlobalPass : public DsaGlobalPass {
  Graph::SetFactory m_setFactory;
  std::unique_ptr<BottomUpGlobalAnalysis> m_ga;

public:
  static char ID;

  BottomUpGlobalPass();

  void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;

  bool runOnModule(llvm::Module &M) override;

  llvm::StringRef getPassName() const override {
    return "Bottom-up SeaDsa pass";
  }

  GlobalAnalysis &getGlobalAnalysis() override {
    return *(static_cast<GlobalAnalysis *>(&*m_ga));
  }
};

// Execute operation Op on each callsite until no more changes
template <class GlobalAnalysis, class Op> class CallGraphClosure {

  GlobalAnalysis &m_ga;
  CallGraphWrapper &m_dsaCG;
  WorkList<DsaCallSite> m_w;

  void exec_callsite(const DsaCallSite &cs, Graph &calleeG, Graph &callerG);

public:
  CallGraphClosure(GlobalAnalysis &ga, CallGraphWrapper &dsaCG)
      : m_ga(ga), m_dsaCG(dsaCG) {}

  bool runOnModule(llvm::Module &M);
};

// Propagate unique scalar flag across callsites
class UniqueScalar {
  CallGraphWrapper &m_dsaCG;
  WorkList<DsaCallSite> &m_w;

public:
  UniqueScalar(CallGraphWrapper &dsaCG, WorkList<DsaCallSite> &w)
      : m_dsaCG(dsaCG), m_w(w) {}

  void runOnCallSite(const DsaCallSite &cs, Node &calleeN, Node &callerN);
};

// Propagate allocation sites across callsites
class AllocaSite {
  CallGraphWrapper &m_dsaCG;
  WorkList<DsaCallSite> &m_w;

public:
  AllocaSite(CallGraphWrapper &dsaCG, WorkList<DsaCallSite> &w)
      : m_dsaCG(dsaCG), m_w(w) {}

  void runOnCallSite(const DsaCallSite &cs, Node &calleeN, Node &callerN);
};
} // namespace sea_dsa
#endif
