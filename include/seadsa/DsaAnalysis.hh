#ifndef __DSA_ANALYSIS_HH_
#define __DSA_ANALYSIS_HH_

#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"

#include <memory>

#include "seadsa/Global.hh"
#include "seadsa/Graph.hh"
#include "seadsa/Info.hh"

/* Entry point for all Dsa clients */

namespace llvm {
class DataLayout;
class TargetLibraryInfoWrapperPass;
class TargetLibraryInfo;
class CallGraph;
class Value;
class Function;
} // namespace llvm

namespace seadsa {
class AllocWrapInfo;
class DsaLibFuncInfo;

class DsaAnalysis : public llvm::ModulePass {

  const llvm::DataLayout *m_dl;
  llvm::TargetLibraryInfoWrapperPass *m_tliWrapper;
  const AllocWrapInfo *m_allocInfo;
  const DsaLibFuncInfo *m_dsaLibFuncInfo;
  Graph::SetFactory m_setFactory;
  std::unique_ptr<GlobalAnalysis> m_ga;
  bool m_print_stats;

public:
  static char ID;

  DsaAnalysis(bool print_stats = false)
      : ModulePass(ID), m_dl(nullptr), m_tliWrapper(nullptr),
        m_allocInfo(nullptr), m_dsaLibFuncInfo(nullptr), m_ga(nullptr),
        m_print_stats(print_stats) {}

  void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;

  bool runOnModule(llvm::Module &M) override;

  llvm::StringRef getPassName() const override {
    return "SeaHorn Dsa analysis: entry point for all clients";
  }

  const llvm::DataLayout &getDataLayout();

  llvm::TargetLibraryInfoWrapperPass &getTLIWrapper() {
    assert(m_tliWrapper);
    return *m_tliWrapper;
  }
  const llvm::TargetLibraryInfo &getTLI(const llvm::Function &F);

  GlobalAnalysis &getDsaAnalysis();
};
} // namespace seadsa

namespace seadsa {

/// Self-contained DsaInfo for new-PM consumers: owns the global analysis and the
/// supporting info objects it depends on, built without the legacy pass manager.
/// Construct with the module and a caller-owned TLI wrapper, then query
/// getDsaInfo() (valid for the lifetime of this object).
class LocalDsaInfo {
  std::unique_ptr<AllocWrapInfo> m_allocInfo;
  std::unique_ptr<DsaLibFuncInfo> m_dsaLibFuncInfo;
  std::unique_ptr<llvm::CallGraph> m_cg;
  Graph::SetFactory m_setFactory;
  std::unique_ptr<GlobalAnalysis> m_ga;
  std::unique_ptr<DsaInfo> m_info;

public:
  LocalDsaInfo(llvm::Module &M,
               llvm::TargetLibraryInfoWrapperPass &tliWrapper);
  ~LocalDsaInfo();
  DsaInfo &getDsaInfo() { return *m_info; }
};

llvm::Pass *createDsaInfoPass();
llvm::Pass *createDsaPrintStatsPass();
llvm::Pass *createDsaPrinterPass();
llvm::Pass *createDsaViewerPass();
llvm::Pass *createDsaPrintCallGraphStatsPass();
llvm::Pass *createDsaCallGraphPrinterPass();
} // namespace seadsa

#endif
