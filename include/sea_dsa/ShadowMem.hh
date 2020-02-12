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
class CallInst;
class ImmutableCallSite;
} // namespace llvm

namespace sea_dsa {
class ShadowMemImpl;
class GlobalAnalysis;
class AllocSiteInfo;
class Cell;
} // namespace sea_dsa

namespace sea_dsa {

enum class ShadowMemInstOp {
  LOAD,        /* load */
  TRSFR_LOAD,  /* memory transfer load */
  STORE,       /* store */
  GLOBAL_INIT, /* initialization of global values */
  INIT,        /* initialization of local shadow variables */
  ARG_INIT,    /* initialization of shadow formal parameters */
  ARG_REF,     /* input actual parameter */
  ARG_MOD,     /* input/output actual parameter */
  ARG_NEW,     /* output actual parameter */
  FUN_IN,      /* input formal parameter */
  FUN_OUT,     /* output formal parameter */
  UNKNOWN
};

class ShadowMem {
  std::unique_ptr<ShadowMemImpl> m_impl;

public:
  ShadowMem(GlobalAnalysis &dsa, AllocSiteInfo &asi,
            llvm::TargetLibraryInfo &tli, llvm::CallGraph *cg,
            llvm::Pass &pass /* for dominatorTree and assumptionCache*/,
            bool splitDsaNodes = false, bool computeReadMod = false,
            bool memOptimizer = true, bool useTBAA = true);

  bool runOnModule(llvm::Module &M);

  // Return a reference to the global sea-dsa analysis.
  GlobalAnalysis &getDsaAnalysis();

  // Return true if Dsa nodes are split by fields (i.e., whether
  // ShadowMem is field-sensitivity or not)
  bool splitDsaNodes() const;

  // Return the id of the field pointed by the given cell c.
  llvm::Optional<unsigned> getCellId(const Cell &c) const;
  
  ShadowMemInstOp getShadowMemOp(const llvm::CallInst &ci) const;

  // Return cell associated to the shadow mem call instruction.
  llvm::Optional<Cell> getShadowMemCell(const llvm::CallInst &ci) const;

  // Return a pair <def,use> with the defined and used variable by the
  // shadow mem instruction. If the instruction does not define or use
  // a variable the corresponding pair element can be null.
  std::pair<llvm::Value *, llvm::Value *>
  getShadowMemVars(llvm::CallInst &ci) const;
};

class ShadowMemPass : public llvm::ModulePass {
  std::unique_ptr<ShadowMem> m_shadowMem;

public:
  static char ID;
  ShadowMemPass();
  bool runOnModule(llvm::Module &M) override;
  void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;
  llvm::StringRef getPassName() const override { return "ShadowMemSeaDsa"; }
  const ShadowMem &getShadowMem() const;
  ShadowMem &getShadowMem();
};

class StripShadowMemPass : public llvm::ModulePass {
public:
  static char ID;
  StripShadowMemPass() : llvm::ModulePass(ID) {}
  virtual bool runOnModule(llvm::Module &M) override;
  virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;
  virtual llvm::StringRef getPassName() const override { return "StripShadowMem"; }
};

llvm::Pass *createStripShadowMemPass();
llvm::Pass *createShadowMemPass();

// bool isShadowMemInst(const llvm::Value *v);
} // namespace sea_dsa
