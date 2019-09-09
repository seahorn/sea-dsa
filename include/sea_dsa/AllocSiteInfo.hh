/**
   A pass to identify allocations sites and their sizes (if known). The
   information is exposed metadata attached to allocation sites.
 */
#pragma once
#include "llvm/ADT/Optional.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Pass.h"

namespace llvm {
class DataLayout;
class Function;
class LLVMContext;
class Module;
class Instruction;
class TargetLibraryInfo;
class Type;
class Value;
} // namespace llvm

namespace sea_dsa {
class AllocWrapInfo;

class AllocSiteInfo : public llvm::ModulePass {
  llvm::TargetLibraryInfo *m_tli;
  const llvm::DataLayout *m_dl;
  AllocWrapInfo *m_awi;

  const llvm::StringRef m_allocSiteMetadataTag = getAllocSiteMetadataTag();

  llvm::Optional<unsigned> maybeEvalAllocSize(llvm::Value &v,
                                              llvm::LLVMContext &ctx);
  bool markAllocs(llvm::Function &F);

  void markAsAllocSite(llvm::Instruction &inst,
                       llvm::Optional<unsigned> allocatedBytes = llvm::None);

public:
  static char ID;

  AllocSiteInfo()
      : ModulePass(ID), m_tli(nullptr), m_dl(nullptr), m_awi(nullptr) {}
  bool runOnModule(llvm::Module &) override;
  llvm::StringRef getPassName() const override {
    return "Allocation Site Identification pass";
  }

  void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;

  static llvm::StringRef getAllocSiteMetadataTag() {
    return "sea.dsa.allocsite";
  };

  static bool isAllocSite(const llvm::Value &v);
  static llvm::Optional<unsigned>
  getAllocSiteSize(const llvm::Value &v);
};
} // namespace sea_dsa
