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
#include <unordered_map>

namespace llvm {
class Function;
class Module;
class TargetLibraryInfoWrapperPass;
class Function;
} // namespace llvm

namespace seadsa {
class LocalAnalysis;
class AllocWrapInfo;

class SpecGraphInfo : public llvm::ImmutablePass {

protected:
  mutable AllocWrapInfo *m_awi;
  mutable llvm::TargetLibraryInfoWrapperPass *m_tliWrapper;
  mutable std::unordered_map<std::string, llvm::Function &> m_funcs;

  // m_module must be deallocated before m_ctx, keep in this order
  mutable llvm::LLVMContext m_ctx;
  mutable std::unique_ptr<llvm::Module> m_module;

public:
  static char ID;

  SpecGraphInfo() : ImmutablePass(ID), m_awi(nullptr), m_tliWrapper(nullptr) {}
  void initialize() const;
  void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;
  llvm::StringRef getPassName() const override { return "SeaDsa Spec Pass"; }

  bool hasSpecFunc(const llvm::Function &F) const;
  llvm::Function &getSpecFunc(const llvm::Function &F) const;
};

llvm::Pass *createSpecGraphInfoPass();

} // namespace seadsa
