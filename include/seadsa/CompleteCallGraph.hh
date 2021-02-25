#pragma once

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"

#include "seadsa/CallSite.hh"
#include "seadsa/Graph.hh"

#include <memory>
#include <vector>

namespace llvm {
class DataLayout;
class TargetLibraryInfo;
class TargetLibraryInfoWrapperPass;
class CallGraph;
class raw_ostream;
} // namespace llvm

namespace seadsa {
class AllocWrapInfo;
class DsaLibFuncInfo;
class Cloner;
class CompleteCallGraph;

class CompleteCallGraphAnalysis {
  friend class CompleteCallGraph;

public:
  using GraphRef = std::shared_ptr<Graph>;
  using GraphMap = llvm::DenseMap<const llvm::Function *, GraphRef>;
  using FunctionVector = std::vector<const llvm::Function *>;
  using callee_iterator = FunctionVector::iterator;

private:
  using CalleesMap = llvm::DenseMap<const llvm::Instruction *, FunctionVector>;

  /// -- for building complete call graph
  const llvm::DataLayout &m_dl;
  llvm::TargetLibraryInfoWrapperPass &m_tliWrapper;
  Graph::SetFactory m_setFactory;
  const AllocWrapInfo &m_allocInfo;
  const DsaLibFuncInfo &m_dsaLibFuncInfo;
  llvm::CallGraph &m_cg;
  std::unique_ptr<llvm::CallGraph> m_complete_cg;
  // true if assume that stack allocated memory does not escape
  bool m_noescape;

  /// -- for client queries
  std::set<const llvm::Instruction *> m_resolved;
  CalleesMap m_callees;

  void mergeGraphs(Graph &fromG, Graph &toG);
  void cloneCallSites(Cloner &C, Graph &calleeG, Graph &callerG);
  void cloneAndResolveArgumentsAndCallSites(const DsaCallSite &CS,
                                            Graph &calleeG, Graph &callerG,
                                            bool noescape = true);

public:
  CompleteCallGraphAnalysis(const llvm::DataLayout &dl,
                            llvm::TargetLibraryInfoWrapperPass &tliWrapper,
                            const AllocWrapInfo &allocInfo,
                            const DsaLibFuncInfo &dsaLibFuncInfo,
                            llvm::CallGraph &cg,
                            bool noescape = true /* TODO: CLI*/);

  bool runOnModule(llvm::Module &M);

  /*
     Methods to return a complete call graph.

     The analysis tries to resolve all indirect calls. If it succeeds
     then the call graph is complete in the sense that all callees are
     statically known. However, the analysis might not succeed if
     external functions take the addresses of some of the functions
     defined in the module.
   */
  llvm::CallGraph &getCompleteCallGraphRef();

  const llvm::CallGraph &getCompleteCallGraphRef() const;

  // this method passes ownership of m_complete_cg to the caller.
  std::unique_ptr<llvm::CallGraph> getCompleteCallGraph();

  void printStats(llvm::Module &m, llvm::raw_ostream &o);

  // The indirect call CB is fully resolved.
  bool isComplete(llvm::CallBase &CB) const;

  // Iterate over all possible callees of an indirect call CS.
  callee_iterator begin(llvm::CallBase &CB);

  callee_iterator end(llvm::CallBase &CB);
};

class CompleteCallGraph : public llvm::ModulePass {
private:
  using CalleesMap = typename CompleteCallGraphAnalysis::CalleesMap;
  using FunctionVector = typename CalleesMap::mapped_type;

public:
  using callee_iterator = FunctionVector::iterator;

private:
  std::unique_ptr<CompleteCallGraphAnalysis> m_CCGA;
  bool m_printStats;

public:
  static char ID;

  CompleteCallGraph(bool printStats = false);

  void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;

  bool runOnModule(llvm::Module &M) override;

  llvm::StringRef getPassName() const override {
    return "Complete Call Graph SeaDsa pass";
  }

  llvm::CallGraph &getCompleteCallGraph();

  const llvm::CallGraph &getCompleteCallGraph() const;

  bool isComplete(llvm::CallBase &CB) const;

  callee_iterator begin(llvm::CallBase &CB);

  callee_iterator end(llvm::CallBase &CB);
};

} // namespace seadsa
