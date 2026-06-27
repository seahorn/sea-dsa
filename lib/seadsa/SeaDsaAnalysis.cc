#include "seadsa/SeaDsaAnalysis.hh"

#include "seadsa/DsaAnalysis.hh" // mkGlobalAnalysis

#include "llvm/ADT/Triple.h"
#include "llvm/IR/Module.h"

using namespace llvm;

namespace seadsa {

// Each AnalysisInfoMixin-derived analysis needs a unique key object; its
// address identifies the analysis in the AnalysisManager.
AnalysisKey AllocWrapInfoAnalysis::Key;
AnalysisKey DsaLibFuncInfoAnalysis::Key;
AnalysisKey DsaInfoAnalysis::Key;

AllocWrapInfoAnalysis::Result
AllocWrapInfoAnalysis::run(Module &M, ModuleAnalysisManager &) {
  auto tli =
      std::make_unique<TargetLibraryInfoWrapperPass>(Triple(M.getTargetTriple()));
  auto awi = std::make_unique<AllocWrapInfo>(tli.get());
  awi->initialize(M, /*legacy Pass=*/nullptr);
  return Result(std::move(tli), std::move(awi));
}

DsaLibFuncInfoAnalysis::Result
DsaLibFuncInfoAnalysis::run(Module &M, ModuleAnalysisManager &) {
  auto dlfi = std::make_unique<DsaLibFuncInfo>();
  dlfi->initialize(M);
  return Result(std::move(dlfi));
}

DsaInfoAnalysis::Result
DsaInfoAnalysis::run(Module &M, ModuleAnalysisManager &MAM) {
  // -- Pull the dependency analyses from the manager; if another consumer
  // -- already requested them this turn, these are cache hits.
  AllocWrapInfo &awi =
      MAM.getResult<AllocWrapInfoAnalysis>(M).getAllocWrapInfo();
  DsaLibFuncInfo &dlfi =
      MAM.getResult<DsaLibFuncInfoAnalysis>(M).getDsaLibFuncInfo();

  const DataLayout &dl = M.getDataLayout();
  auto tli =
      std::make_unique<TargetLibraryInfoWrapperPass>(Triple(M.getTargetTriple()));
  auto cg = std::make_unique<CallGraph>(M);
  auto sf = std::make_unique<Graph::SetFactory>();

  auto ga = mkGlobalAnalysis(dl, *tli, awi, dlfi, *cg, *sf);
  ga->runOnModule(M);

  auto info = std::make_unique<DsaInfo>(dl, *tli, *ga);
  info->runOnModule(M);

  return Result(std::move(tli), std::move(cg), std::move(sf), std::move(ga),
                std::move(info));
}

} // namespace seadsa
