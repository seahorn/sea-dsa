#include "seadsa/SeaDsaAnalysis.hh"

#include "seadsa/DsaAnalysis.hh" // mkGlobalAnalysis

#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/IR/Module.h"

using namespace llvm;

namespace seadsa {

AnalysisKey AllocWrapInfoAnalysis::Key;
AnalysisKey DsaLibFuncInfoAnalysis::Key;
AnalysisKey DsaInfoAnalysis::Key;

// Per-function TLI from the new-PM TargetLibraryAnalysis (via the FAM proxy):
// no legacy TargetLibraryInfoWrapperPass anywhere.
static TargetLibraryInfoGetter mkTLIGetter(Module &M, ModuleAnalysisManager &MAM) {
  auto &FAM =
      MAM.getResult<FunctionAnalysisManagerModuleProxy>(M).getManager();
  return [&FAM](const Function &F) -> const TargetLibraryInfo & {
    return FAM.getResult<TargetLibraryAnalysis>(const_cast<Function &>(F));
  };
}

AllocWrapInfoAnalysis::Result
AllocWrapInfoAnalysis::run(Module &M, ModuleAnalysisManager &MAM) {
  auto awi = std::make_unique<AllocWrapInfo>(mkTLIGetter(M, MAM));
  awi->initialize(M, /*legacy Pass=*/nullptr);
  return Result(std::move(awi));
}

DsaLibFuncInfoAnalysis::Result
DsaLibFuncInfoAnalysis::run(Module &M, ModuleAnalysisManager &) {
  auto dlfi = std::make_unique<DsaLibFuncInfo>();
  dlfi->initialize(M);
  return Result(std::move(dlfi));
}

DsaInfoAnalysis::Result
DsaInfoAnalysis::run(Module &M, ModuleAnalysisManager &MAM) {
  AllocWrapInfo &awi =
      MAM.getResult<AllocWrapInfoAnalysis>(M).getAllocWrapInfo();
  DsaLibFuncInfo &dlfi =
      MAM.getResult<DsaLibFuncInfoAnalysis>(M).getDsaLibFuncInfo();

  TargetLibraryInfoGetter getTLI = mkTLIGetter(M, MAM);
  const DataLayout &dl = M.getDataLayout();
  auto cg = std::make_unique<CallGraph>(M);
  auto sf = std::make_unique<Graph::SetFactory>();

  auto ga = mkGlobalAnalysis(dl, getTLI, awi, dlfi, *cg, *sf);
  ga->runOnModule(M);

  auto info = std::make_unique<DsaInfo>(dl, getTLI, *ga);
  info->runOnModule(M);

  return Result(std::move(cg), std::move(sf), std::move(ga), std::move(info));
}

} // namespace seadsa
