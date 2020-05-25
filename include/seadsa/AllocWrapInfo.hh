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
class LoopInfo;
class Module;
class TargetLibraryInfo;
class TargetLibraryInfoWrapperPass;
class Function;
class Value;
} // namespace llvm

namespace seadsa {
class AllocWrapInfo : public llvm::ImmutablePass {
protected:
  mutable std::set<std::string> m_allocs;
  mutable std::set<std::string> m_deallocs;

  mutable llvm::TargetLibraryInfoWrapperPass *m_tliWrapper;

  void findAllocs(llvm::Module &M) const;
  bool findWrappers(llvm::Module &M, Pass *P, std::set<std::string> &fn_names) const; 
  bool flowsFrom(llvm::Value *, llvm::Value *, llvm::LoopInfo *) const;

  
public:
  static char ID;
  AllocWrapInfo(): ImmutablePass(ID), m_tliWrapper(nullptr) {}

  void initialize(llvm::Module &, Pass *) const;
  
  llvm::StringRef getPassName() const override {
    return "Find malloc/free wrapper functions";
  }
  void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;

  bool isAllocWrapper(llvm::Function &) const;
  bool isDeallocWrapper(llvm::Function &) const;
  const std::set<std::string> &getAllocWrapperNames(llvm::Module &) const;


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
