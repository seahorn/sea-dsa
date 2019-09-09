/**
   A pass to identify alloc/dealloc function wrappers. Based on
   AllocatorIdentification.h from llvm-dsa
 */
#pragma once
#include "llvm/ADT/StringRef.h"
#include "llvm/Pass.h"

#include <set>
#include <string>

namespace llvm {
class Function;
class Module;
class Instruction;
class TargetLibraryInfo;
class Value;
} // namespace llvm

namespace sea_dsa {
class AllocWrapInfo : public llvm::ModulePass {
protected:
  std::set<std::string> m_allocs;
  std::set<std::string> m_deallocs;

  llvm::TargetLibraryInfo *m_tli;

  void findAllocs(llvm::Module &M);
  bool findWrappers(llvm::Module &M, std::set<std::string> &fn_names);
  bool flowsFrom(llvm::Value *, llvm::Value *);

public:
  static char ID;
  AllocWrapInfo() : ModulePass(ID), m_tli(nullptr) {}

  bool runOnModule(llvm::Module &) override;
  llvm::StringRef getPassName() const override {
    return "Find malloc/free wrapper functions";
  }
  void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;

  bool isAllocWrapper(llvm::Function &) const;
  bool isDeallocWrapper(llvm::Function &) const;
  const std::set<std::string> &getAllocWrapperNames() const { return m_allocs; }

  const llvm::TargetLibraryInfo &getTLI() const {
    assert(m_tli);
    return *m_tli;
  }
};
} // namespace sea_dsa
