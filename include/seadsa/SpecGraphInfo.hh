/**
   A pass to replace bottom-up PTA of identified functions with a pre-generated
   spec
 */
#pragma once

#include "llvm/ADT/StringRef.h"
#include "llvm/Pass.h"

#include "seadsa/Local.hh"

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"

namespace llvm {
class Function;
class Module;
class Instruction;
class TargetLibraryInfo;
class TargetLibraryInfoWrapperPass;
class Function;
class Value;
class StringRef;
} // namespace llvm

namespace seadsa {
class Local;

class SpecGraphInfo : public llvm::ModulePass {

  Local m_local;
  llvm::StringRef m_specDir;
  // populateSpecSet();

public:
  static char ID;

  SpecGraphInfo() : ModulePass(ID), m_local() {
    // populateSpecSet();
  }

  bool runOnModule(llvm::Module &m);

  bool hasFunction() const;
  const Graph &getGraph(const llvm::Function &F) const;
};
llvm::Pass *createSpecGraphInfoPass();

} // namespace seadsa