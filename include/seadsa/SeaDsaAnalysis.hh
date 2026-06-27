#ifndef SEADSA_SEA_DSA_ANALYSIS_HH
#define SEADSA_SEA_DSA_ANALYSIS_HH
/// New-pass-manager analyses for sea-dsa.
///
/// These wrap sea-dsa's (legacy-shaped) analysis objects as real new-PM module
/// analyses (llvm::AnalysisInfoMixin). The ModuleAnalysisManager then computes
/// and *caches* each one per module and hands the same result to every
/// consumer that calls getResult<> -- which is the whole point of an analysis
/// manager (compute-once / share / invalidate).
///
/// They also illustrate analysis-to-analysis composition: DsaInfoAnalysis::run
/// pulls AllocWrapInfo and DsaLibFuncInfo back out of the MAM via getResult,
/// instead of rebuilding them. If two passes both request DsaInfoAnalysis, the
/// expensive points-to analysis runs once.
///
/// Note: sea-dsa predates the new-PM TargetLibraryAnalysis and consumes the
/// legacy TargetLibraryInfoWrapperPass *object*, so each result owns a wrapper
/// built from the module triple. A deeper refactor would teach sea-dsa to take
/// a TLI getter (function_ref) and drop the wrapper entirely.
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/IR/PassManager.h"

#include "seadsa/AllocWrapInfo.hh"
#include "seadsa/DsaLibFuncInfo.hh"
#include "seadsa/Global.hh"
#include "seadsa/Graph.hh"
#include "seadsa/Info.hh"

#include <memory>

namespace seadsa {

/// Cached, initialized AllocWrapInfo.
class AllocWrapInfoAnalysis
    : public llvm::AnalysisInfoMixin<AllocWrapInfoAnalysis> {
  friend llvm::AnalysisInfoMixin<AllocWrapInfoAnalysis>;
  static llvm::AnalysisKey Key;

public:
  class Result {
    std::unique_ptr<llvm::TargetLibraryInfoWrapperPass> m_tliWrapper;
    std::unique_ptr<AllocWrapInfo> m_awi;

  public:
    Result(std::unique_ptr<llvm::TargetLibraryInfoWrapperPass> tli,
           std::unique_ptr<AllocWrapInfo> awi)
        : m_tliWrapper(std::move(tli)), m_awi(std::move(awi)) {}
    AllocWrapInfo &getAllocWrapInfo() { return *m_awi; }
    bool invalidate(llvm::Module &, const llvm::PreservedAnalyses &PA,
                    llvm::ModuleAnalysisManager::Invalidator &) {
      auto PAC = PA.getChecker<AllocWrapInfoAnalysis>();
      return !(PAC.preserved() ||
               PAC.preservedSet<llvm::AllAnalysesOn<llvm::Module>>());
    }
  };

  Result run(llvm::Module &M, llvm::ModuleAnalysisManager &MAM);
};

/// Cached, initialized DsaLibFuncInfo.
class DsaLibFuncInfoAnalysis
    : public llvm::AnalysisInfoMixin<DsaLibFuncInfoAnalysis> {
  friend llvm::AnalysisInfoMixin<DsaLibFuncInfoAnalysis>;
  static llvm::AnalysisKey Key;

public:
  class Result {
    std::unique_ptr<DsaLibFuncInfo> m_dlfi;

  public:
    explicit Result(std::unique_ptr<DsaLibFuncInfo> dlfi)
        : m_dlfi(std::move(dlfi)) {}
    DsaLibFuncInfo &getDsaLibFuncInfo() { return *m_dlfi; }
    bool invalidate(llvm::Module &, const llvm::PreservedAnalyses &PA,
                    llvm::ModuleAnalysisManager::Invalidator &) {
      auto PAC = PA.getChecker<DsaLibFuncInfoAnalysis>();
      return !(PAC.preserved() ||
               PAC.preservedSet<llvm::AllAnalysesOn<llvm::Module>>());
    }
  };

  Result run(llvm::Module &M, llvm::ModuleAnalysisManager &MAM);
};

/// Cached DsaInfo (sea-dsa's points-to summary). Depends on
/// AllocWrapInfoAnalysis + DsaLibFuncInfoAnalysis (fetched from the MAM) and
/// owns the global analysis it is built on.
class DsaInfoAnalysis : public llvm::AnalysisInfoMixin<DsaInfoAnalysis> {
  friend llvm::AnalysisInfoMixin<DsaInfoAnalysis>;
  static llvm::AnalysisKey Key;

public:
  class Result {
    std::unique_ptr<llvm::TargetLibraryInfoWrapperPass> m_tliWrapper;
    std::unique_ptr<llvm::CallGraph> m_cg;
    std::unique_ptr<Graph::SetFactory> m_setFactory;
    std::unique_ptr<GlobalAnalysis> m_ga;
    std::unique_ptr<DsaInfo> m_info;

  public:
    Result(std::unique_ptr<llvm::TargetLibraryInfoWrapperPass> tli,
           std::unique_ptr<llvm::CallGraph> cg,
           std::unique_ptr<Graph::SetFactory> sf,
           std::unique_ptr<GlobalAnalysis> ga, std::unique_ptr<DsaInfo> info)
        : m_tliWrapper(std::move(tli)), m_cg(std::move(cg)),
          m_setFactory(std::move(sf)), m_ga(std::move(ga)),
          m_info(std::move(info)) {}
    DsaInfo &getDsaInfo() { return *m_info; }
    bool invalidate(llvm::Module &, const llvm::PreservedAnalyses &PA,
                    llvm::ModuleAnalysisManager::Invalidator &) {
      auto PAC = PA.getChecker<DsaInfoAnalysis>();
      return !(PAC.preserved() ||
               PAC.preservedSet<llvm::AllAnalysesOn<llvm::Module>>());
    }
  };

  Result run(llvm::Module &M, llvm::ModuleAnalysisManager &MAM);
};

} // namespace seadsa

#endif
