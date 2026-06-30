/**
   A pass to identify alloc/dealloc function wrappers. Based on
   AllocatorIdentification.h from llvm-dsa
 */
#pragma once
#include "seadsa/TargetLibraryInfoGetter.hh"
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
  mutable std::set<llvm::StringRef> m_allocs;
  mutable std::set<llvm::StringRef> m_deallocs;

  mutable seadsa::TargetLibraryInfoGetter m_getTLI;

  void findAllocs(llvm::Module &M) const;
  bool findWrappers(llvm::Module &M, Pass *P,
                    std::set<llvm::StringRef> &fn_names) const;
  bool flowsFrom(llvm::Value *, llvm::Value *, llvm::LoopInfo *) const;

  
public:
  static char ID;
  AllocWrapInfo(seadsa::TargetLibraryInfoGetter getTLI = nullptr)
    : ImmutablePass(ID), m_getTLI(getTLI) {}

  // P is used to call LoopInfoWrapperPass.
  // It can be null if the client of AllocWrapInfo is an immutable
  // pass (e.g.,SeaDsaAAWraperPass). 
  void initialize(llvm::Module &, Pass *P) const;
  
  llvm::StringRef getPassName() const override {
    return "Find malloc/free wrapper functions";
  }
  void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;

  bool isAllocWrapper(llvm::Function &) const;
  bool isDeallocWrapper(llvm::Function &) const;
  const std::set<llvm::StringRef> &getAllocWrapperNames(llvm::Module &) const;


  const llvm::TargetLibraryInfo &getTLI(const llvm::Function &F) const {
    assert(m_getTLI);
    return m_getTLI(F);
  }
};

} // namespace seadsa
