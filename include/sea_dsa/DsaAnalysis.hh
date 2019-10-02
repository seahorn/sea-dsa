#ifndef __DSA_ANALYSIS_HH_
#define __DSA_ANALYSIS_HH_

#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"

#include "sea_dsa/Global.hh"
#include "sea_dsa/Graph.hh"
#include "sea_dsa/Info.hh"

/* Entry point for all Dsa clients */

namespace llvm {
class DataLayout;
class TargetLibraryInfo;
class CallGraph;
class Value;
} // namespace llvm

namespace sea_dsa {
class AllocWrapInfo;

class DsaAnalysis : public llvm::ModulePass {

  const llvm::DataLayout *m_dl;
  llvm::TargetLibraryInfo *m_tli;
  const AllocWrapInfo *m_allocInfo;
  Graph::SetFactory m_setFactory;
  std::unique_ptr<GlobalAnalysis> m_ga;
  bool m_print_stats;

public:
  static char ID;

  DsaAnalysis(bool print_stats = false)
      : ModulePass(ID), m_dl(nullptr), m_tli(nullptr), m_allocInfo(nullptr),
        m_ga(nullptr), m_print_stats(print_stats) {}

  void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;

  bool runOnModule(llvm::Module &M) override;

  llvm::StringRef getPassName() const override {
    return "SeaHorn Dsa analysis: entry point for all clients";
  }

  const llvm::DataLayout &getDataLayout();

  const llvm::TargetLibraryInfo &getTLI();

  GlobalAnalysis &getDsaAnalysis();
};
} // namespace sea_dsa

namespace sea_dsa {
llvm::Pass *createDsaInfoPass();
llvm::Pass *createDsaPrintStatsPass();
llvm::Pass *createDsaPrinterPass();
llvm::Pass *createDsaViewerPass();
llvm::Pass *createDsaPrintCallGraphStatsPass();
llvm::Pass *createDsaCallGraphPrinterPass();
} // namespace sea_dsa

#endif
