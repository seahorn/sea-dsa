#pragma once

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"

#include "sea_dsa/CallSite.hh"
#include "sea_dsa/Graph.hh"

#include <vector>
#include <memory>

namespace llvm {
class DataLayout;
class TargetLibraryInfo;
class CallGraph;
} // namespace llvm

namespace sea_dsa {
class AllocWrapInfo;
class Cloner;

class CompleteCallGraphAnalysis {

public:
  typedef std::shared_ptr<Graph> GraphRef;
  typedef llvm::DenseMap<const llvm::Function *, GraphRef> GraphMap;
  typedef std::vector<const llvm::Function*> FunctionVector;
  
private:
  const llvm::DataLayout &m_dl;
  const llvm::TargetLibraryInfo &m_tli;
  const AllocWrapInfo &m_allocInfo;
  llvm::CallGraph &m_cg;
  std::unique_ptr<llvm::CallGraph> m_complete_cg;
  // true if assume that alloca allocated (stack) memory does not escape
  bool m_noescape;

  void mergeGraphs(Graph &fromG, Graph &toG);  
  void cloneCallSites(Cloner& C, Graph &calleeG, Graph &callerG);
  void cloneAndResolveArgumentsAndCallSites(const DsaCallSite &CS, Graph &calleeG,
					    Graph &callerG, bool noescape = true);
  
public:

  CompleteCallGraphAnalysis(const llvm::DataLayout &dl,
                   const llvm::TargetLibraryInfo &tli,
                   const AllocWrapInfo &allocInfo, llvm::CallGraph &cg,
                   bool noescape = true /* TODO: CLI*/);

  bool runOnModule(llvm::Module &M, GraphMap &graphs);

  /* 
     Methods to return a complete call graph. 
     
     The analysis tries to resolve all indirect calls. If it succeeds
     then the call graph is complete in the sense that all callees are
     statically known. However, the analysis might not succeed if
     external functions take the addresses of some of the functions
     defined in the module.
   */
  llvm::CallGraph& getCompleteCallGraphRef();
  
  const llvm::CallGraph& getCompleteCallGraphRef() const;
  
  // this method passes ownership of m_complete_cg to the caller.
  std::unique_ptr<llvm::CallGraph> getCompleteCallGraph();
};

class CompleteCallGraph : public llvm::ModulePass {
  typedef typename CompleteCallGraphAnalysis::GraphRef GraphRef;
  typedef typename CompleteCallGraphAnalysis::GraphMap GraphMap;

  Graph::SetFactory m_setFactory;
  const llvm::DataLayout *m_dl;
  const llvm::TargetLibraryInfo *m_tli;
  const AllocWrapInfo *m_allocInfo;
  GraphMap m_graphs;
  std::unique_ptr<llvm::CallGraph> m_complete_cg;

public:
  static char ID;

  CompleteCallGraph();

  void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;

  bool runOnModule(llvm::Module &M) override;

  llvm::StringRef getPassName() const override { return "Complete Call Graph SeaDsa pass"; }

  llvm::CallGraph& getCompleteCallGraph();
  
  const llvm::CallGraph& getCompleteCallGraph() const;  
};

} // namespace sea_dsa
