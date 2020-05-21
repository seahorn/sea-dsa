/**
   A pass to identify alloc/dealloc function wrappers. Based on
   AllocatorIdentification.h from llvm-dsa
 */
#pragma once
#include "llvm/ADT/StringRef.h"
#include "llvm/Pass.h"
#include "llvm/Analysis/TargetLibraryInfo.h"

#include <set>
#include <string>

namespace llvm {
class Function;
class Module;
class Instruction;
class TargetLibraryInfo;
class TargetLibraryInfoWrapperPass;
class Function;
class Value;
} // namespace llvm

namespace seadsa {
class AllocWrapInfo : public llvm::ImmutablePass {
protected:
  std::set<std::string> m_allocs;
  std::set<std::string> m_deallocs;

  llvm::TargetLibraryInfoWrapperPass *m_tliWrapper;

  void findAllocs(llvm::Module &M);
  bool findWrappers(llvm::Module &M, std::set<std::string> &fn_names);
  bool flowsFrom(llvm::Value *, llvm::Value *);

public:
  static char ID;
  AllocWrapInfo() : ImmutablePass(ID), m_tliWrapper(nullptr) {}

  bool runOnModule(llvm::Module &) override;
  llvm::StringRef getPassName() const override {
    return "Find malloc/free wrapper functions";
  }
  void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;

  bool isAllocWrapper(llvm::Function &) const;
  bool isDeallocWrapper(llvm::Function &) const;
  const std::set<std::string> &getAllocWrapperNames() const { return m_allocs; }


  llvm::TargetLibraryInfoWrapperPass &getTLIWrapper() const {
    assert(m_tliWrapper);
    return *m_tliWrapper;
  }
  const llvm::TargetLibraryInfo &getTLI(const llvm::Function &F) const {
    assert(m_tliWrapper);
    return m_tliWrapper->getTLI(F);
  }
};
} // namespace seadsa
