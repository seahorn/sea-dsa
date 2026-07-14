// ==- SeaDsaAliasAnalysis.hh - DSA-based Alias Analysis  ==//

#pragma once
#include "seadsa/TargetLibraryInfoGetter.hh"

#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"

#include "seadsa/Graph.hh"

#include <memory>

namespace llvm {
class CallGraph;
class Function;
class MemoryLocation;
class TargetLibraryInfoWrapper;
} // namespace llvm

namespace seadsa {

class AllocWrapInfo;
class DsaLibFuncInfo;
class BottomUpTopDownGlobalAnalysis;

class SeaDsaAAResult : public llvm::AAResultBase {
  using Base = llvm::AAResultBase;
  friend Base;

public:
  explicit SeaDsaAAResult(seadsa::TargetLibraryInfoGetter getTLI,
                          AllocWrapInfo &AWI, DsaLibFuncInfo &dlfi);

  SeaDsaAAResult(SeaDsaAAResult &&RHS);
  ~SeaDsaAAResult();

  bool invalidate(llvm::Function &F, const llvm::PreservedAnalyses &,
                  llvm::FunctionAnalysisManager::Invalidator &) {
    return false;
  }

  llvm::AliasResult alias(const llvm::MemoryLocation &,
                          const llvm::MemoryLocation &, llvm::AAQueryInfo &,
                          const llvm::Instruction *);

private:
  seadsa::TargetLibraryInfoGetter m_getTLI;
  const llvm::DataLayout *m_dl;
  AllocWrapInfo &m_awi;
  DsaLibFuncInfo &m_dlfi;
  std::unique_ptr<Graph::SetFactory> m_fac; // node factory for seadsa
  std::unique_ptr<llvm::CallGraph> m_cg;
  std::unique_ptr<BottomUpTopDownGlobalAnalysis> m_dsa;
};

class SeaDsaAAWrapperPass : public llvm::ImmutablePass {

  std::unique_ptr<SeaDsaAAResult> Result;

public:
  using AnalysisUsage = llvm::AnalysisUsage;
  static char ID;

  SeaDsaAAWrapperPass();

  SeaDsaAAResult &getResult() { return *Result; }
  const SeaDsaAAResult &getResult() const { return *Result; }

  void initializePass() override;
  void getAnalysisUsage(AnalysisUsage &AU) const override;
};

llvm::ImmutablePass *createSeaDsaAAWrapperPass();

/// New-PM function analysis whose Result is SeaDsaAAResult. Register it in an
/// AAManager ahead of the default pipeline (mirroring the legacy
/// ExternalAAWrapperPass ordering) to expose seadsa to AAEvaluator and other
/// new-PM AA consumers. Requires AllocWrapInfoAnalysis and
/// DsaLibFuncInfoAnalysis results to be cached in the module analysis manager.
class SeaDsaAA : public llvm::AnalysisInfoMixin<SeaDsaAA> {
  friend llvm::AnalysisInfoMixin<SeaDsaAA>;
  static llvm::AnalysisKey Key;

public:
  using Result = SeaDsaAAResult;
  Result run(llvm::Function &F, llvm::FunctionAnalysisManager &FAM);
};
} // namespace seadsa
