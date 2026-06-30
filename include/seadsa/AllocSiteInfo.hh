/**
   A pass to identify allocations sites and their sizes (if known). The
   information is exposed metadata attached to allocation sites.
 */
#pragma once
#include "llvm/ADT/StringRef.h"
#include "llvm/Pass.h"

#include <optional>

namespace llvm {
class DataLayout;
class Function;
class LLVMContext;
class Module;
class Instruction;
class TargetLibraryInfo;
class TargetLibraryInfoWrapperPass;
class Type;
class Value;
} // namespace llvm

namespace seadsa {
class AllocWrapInfo;

class AllocSiteInfo : public llvm::ModulePass {
  const llvm::TargetLibraryInfo *m_tli;
  const llvm::DataLayout *m_dl;
  AllocWrapInfo *m_awi;

  const llvm::StringRef m_allocSiteMetadataTag = getAllocSiteMetadataTag();

  std::optional<unsigned> maybeEvalAllocSize(llvm::Value &v,
                                              llvm::LLVMContext &ctx);
  bool markAllocs(llvm::Function &F);

  void markAsAllocSite(llvm::Instruction &inst,
                       std::optional<unsigned> allocatedBytes = std::nullopt);

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
  static std::optional<unsigned>
  getAllocSiteSize(const llvm::Value &v);
};
} // namespace seadsa
