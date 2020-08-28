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

class DsaLibFuncInfo : public llvm::ImmutablePass {
  using ModuleRef = std::unique_ptr<llvm::Module>;
  using GraphRef = std::shared_ptr<Graph>;

private:
  void initSpecModule() const;

protected:
  mutable bool m_isInitialized = false;
  mutable bool m_genSpecOpt = false;
  mutable llvm::StringMap<llvm::Function *> m_funcs;
  mutable std::vector<ModuleRef> m_modules;
  // m_specModule uses defs from m_specLang so it must be deallocated after
  // m_specModule
  mutable ModuleRef m_specLang;
  mutable ModuleRef m_specModule;

public:
  static char ID;

  DsaLibFuncInfo() : ImmutablePass(ID) {}
  void initialize(const llvm::Module &m) const;
  void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;
  llvm::StringRef getPassName() const override { return "SeaDsa Spec Pass"; }

  bool genSpecs() const { return m_genSpecOpt; }

  bool hasSpecFunc(const llvm::Function &F) const;
  llvm::Function *getSpecFunc(const llvm::Function &F) const;
  void generateSpec(const llvm::Function &F, const GraphRef G) const;
  void writeSpecModule() const;
};

llvm::Pass *createDsaLibFuncInfoPass();

} // namespace seadsa
