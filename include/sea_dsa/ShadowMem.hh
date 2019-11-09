#pragma once
/****
 * Instrument the bitcode with shadow instructions used to construct
 * Memory SSA form.
 ****/

#include "llvm/Pass.h"
#include <memory>

namespace llvm {
class Value;
class Function;
class TargetLibraryInfo;
class DataLayout;
class CallGraph;
}

namespace sea_dsa {
class ShadowMemImpl;
class GlobalAnalysis;
class AllocSiteInfo;
}

namespace sea_dsa {
class ShadowMem {
  std::unique_ptr<ShadowMemImpl> m_impl;
public:
  ShadowMem(GlobalAnalysis& dsa, AllocSiteInfo &asi,
	    llvm::TargetLibraryInfo &tli, llvm::CallGraph *cg,
	    llvm::Pass &pass /* for dominatorTree and assumptionCache*/,
	    bool splitDsaNodes = false, bool computeReadMod = false,
	    bool memOptimizer = true, bool useTBAA = true);
  bool runOnModule(llvm::Module &M);
};

class ShadowMemPass : public llvm::ModulePass {
  std::unique_ptr<ShadowMem> m_shadowMem;
public:
  static char ID;
  ShadowMemPass();
  bool runOnModule(llvm::Module &M) override;
  void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;
  llvm::StringRef getPassName() const override { return "ShadowMemSeaDsa"; }
  const ShadowMem& getShadowMem() const;
};

class StripShadowMemPass : public llvm::ModulePass {
public:
  static char ID;
  StripShadowMemPass(): llvm::ModulePass(ID) {}
  virtual bool runOnModule(llvm::Module &M) override;
  virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;
  virtual llvm::StringRef getPassName() const { return "StripShadowMem"; } 
};

llvm::Pass *createStripShadowMemPass();
llvm::Pass *createShadowMemPass();

//bool isShadowMemInst(const llvm::Value *v);
} // namespace sea_dsa
  
