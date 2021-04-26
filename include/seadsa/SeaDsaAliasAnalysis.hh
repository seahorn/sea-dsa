// ==- SeaDsaAliasAnalysis.hh - DSA-based Alias Analysis  ==//

#pragma once

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

class SeaDsaAAResult : public llvm::AAResultBase<SeaDsaAAResult> {
  using Base = llvm::AAResultBase<SeaDsaAAResult>;
  friend Base;

public:
  explicit SeaDsaAAResult(llvm::TargetLibraryInfoWrapperPass &tliWrapper,
                          AllocWrapInfo &AWI, DsaLibFuncInfo &dlfi);

  SeaDsaAAResult(SeaDsaAAResult &&RHS);
  ~SeaDsaAAResult();

  bool invalidate(llvm::Function &F, const llvm::PreservedAnalyses &,
                  llvm::FunctionAnalysisManager::Invalidator &) {
    return false;
  }

  llvm::AliasResult alias(const llvm::MemoryLocation &,
                          const llvm::MemoryLocation &, llvm::AAQueryInfo &);

private:
  llvm::TargetLibraryInfoWrapperPass &m_tliWrapper;
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
} // namespace seadsa
