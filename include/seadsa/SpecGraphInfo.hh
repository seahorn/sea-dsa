/**
   A pass to replace bottom-up PTA of identified functions with a pre-generated
   spec
 */
#pragma once

#include "llvm/ADT/StringRef.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Pass.h"

#include "seadsa/AllocWrapInfo.hh"
#include "seadsa/Local.hh"

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"

namespace llvm {
class Function;
class Module;
class TargetLibraryInfoWrapperPass;
class Function;
} // namespace llvm

namespace seadsa {
class LocalAnalysis;
class AllocWrapInfo;

class SpecGraphInfo : public llvm::ModulePass {
  using GraphRef = std::shared_ptr<Graph>;

  AllocWrapInfo *m_awi;
  llvm::TargetLibraryInfoWrapperPass *m_tliWrapper;
  Graph::SetFactory m_setFactory;
  llvm::DenseMap<const llvm::Function *, GraphRef> m_graphs;

  void getAvailableSpecs();

public:
  static char ID;

  void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;
  SpecGraphInfo() : ModulePass(ID), m_awi(nullptr), m_tliWrapper(nullptr) {}
  bool runOnModule(llvm::Module &m) override;
  llvm::StringRef getPassName() const override { return "SeaDsa Spec Pass"; }
  bool hasGraph(const llvm::Function &F) const;
  Graph &getGraph(const llvm::Function &F) const;
};

llvm::Pass *createSpecGraphInfoPass();

} // namespace seadsa

// clang-format off
// /home/anton/seahorn_full/seahorn/debug/run/bin/seadsa --sea-dsa=cs --sea-dsa-dot /home/anton/seahorn_full/seahorn/sea-dsa/tests/test-ext-3.ll --sea-dsa-stats --sea-dsa-specdir=/home/anton/seahorn_full/seahorn/sea-dsa/specs