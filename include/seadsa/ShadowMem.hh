#pragma once
/****
 * Instrument the bitcode with shadow instructions used to construct
 * Memory SSA form.
 ****/

#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"

#include <memory>
#include <optional>

#include "seadsa/DsaAnalysis.hh"

namespace llvm {
class Value;
class Function;
class TargetLibraryInfo;
class TargetLibraryInfoWrapperPass;
class DataLayout;
class CallGraph;
class CallInst;
class DominatorTree;
class AssumptionCache;
} // namespace llvm

#include <functional>

namespace seadsa {

using DomTreeGetter = std::function<llvm::DominatorTree &(llvm::Function &)>;
using AssumptionCacheGetter =
    std::function<llvm::AssumptionCache &(llvm::Function &)>;
class ShadowMemImpl;
class GlobalAnalysis;
class AllocSiteInfo;
class Cell;
class SeaMemorySSA;
} // namespace seadsa

namespace seadsa {
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
            llvm::TargetLibraryInfoWrapperPass &tli, llvm::CallGraph *cg,
            llvm::Pass &pass /* for dominatorTree and assumptionCache*/,
            bool splitDsaNodes = false, bool computeReadMod = false,
            bool memOptimizer = true, bool useTBAA = true,
            bool useSNAAA = true);
  /// new-PM construction: analyses provided as getters
  ShadowMem(GlobalAnalysis &dsa, TargetLibraryInfoGetter getTLI,
            llvm::CallGraph *cg, DomTreeGetter getDT,
            AssumptionCacheGetter getAC, bool splitDsaNodes = false,
            bool computeReadMod = false, bool memOptimizer = true,
            bool useTBAA = true, bool useSNAAA = true);

  ~ShadowMem();

  bool runOnModule(llvm::Module &M);

  // Return a reference to the global sea-dsa analysis.
  GlobalAnalysis &getDsaAnalysis();

  // Return true if Dsa nodes are split by fields (i.e., whether
  // ShadowMem is field-sensitivity or not)
  bool splitDsaNodes() const;

  // Return the id of the field pointed by the given cell c.
  std::optional<unsigned> getCellId(const Cell &c) const;

  ShadowMemInstOp getShadowMemOp(const llvm::CallInst &ci) const;

  // Return cell associated to the shadow mem call instruction.
  std::optional<Cell> getShadowMemCell(const llvm::CallInst &ci) const;

  // Return a pair <def,use> with the defined and used variable by the
  // shadow mem instruction. If the instruction does not define or use
  // a variable the corresponding pair element can be null.
  std::pair<llvm::Value *, llvm::Value *>
  getShadowMemVars(llvm::CallInst &ci) const;

  SeaMemorySSA *getMemorySSA(llvm::Function &F);
};

class ShadowMemPass : public llvm::ModulePass {
  std::unique_ptr<ShadowMem> m_shadowMem;

public:
  static char ID;
  ShadowMemPass();
  bool runOnModule(llvm::Module &M) override;
  void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;
  virtual llvm::StringRef getPassName() const override {
    return "ShadowMemSeaDsa";
  }
  const ShadowMem &getShadowMem() const;
  ShadowMem &getShadowMem();
};

class StripShadowMemPass : public llvm::ModulePass {
public:
  static char ID;
  StripShadowMemPass() : llvm::ModulePass(ID) {}
  virtual bool runOnModule(llvm::Module &M) override;
  virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;
  virtual llvm::StringRef getPassName() const override {
    return "StripShadowMem";
  }
};

llvm::Pass *createStripShadowMemPass();

/// New-PM twin of ShadowMemPass: instruments the module with shadow.mem
/// calls, taking sea-dsa's GlobalAnalysis from DsaInfoAnalysis and the
/// per-function analyses from the FAM. Runs AllocSiteInfoAnalysis for its
/// alloc-marking side effect first. Pass a sink to keep the ShadowMem
/// object alive for consumers.
class ShadowMemNewPmPass : public llvm::PassInfoMixin<ShadowMemNewPmPass> {
  std::unique_ptr<ShadowMem> *m_keep;

public:
  ShadowMemNewPmPass(std::unique_ptr<ShadowMem> *keep = nullptr)
      : m_keep(keep) {}
  llvm::PreservedAnalyses run(llvm::Module &M,
                              llvm::ModuleAnalysisManager &MAM);
};

class StripShadowMemNewPmPass : public llvm::PassInfoMixin<StripShadowMemNewPmPass> {
public:
  llvm::PreservedAnalyses run(llvm::Module &M, llvm::ModuleAnalysisManager &MAM);
};
llvm::Pass *createShadowMemPass();

bool isShadowMemInst(const llvm::Value &v);
} // namespace seadsa
