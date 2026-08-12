/// Regression tests for the analysis-preservation contract of
/// ShadowMemNewPmPass.
///
/// The pass mutates the module heavily, so it invalidates essentially
/// everything -- except DsaInfoAnalysis. Its result owns the GlobalAnalysis
/// that the ShadowMem handed to the `keep` sink holds *by reference*, so
/// letting the ModuleAnalysisManager free that result leaves
/// ShadowMem::getDsaAnalysis() dangling. These tests pin both halves of that
/// contract: DsaInfoAnalysis survives, and nothing else does.

#include "doctest.h"

#include "seadsa/Graph.hh"
#include "seadsa/SeaDsaAnalysis.hh"
#include "seadsa/ShadowMem.hh"

#include "llvm/AsmParser/Parser.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/SourceMgr.h"

#include <memory>

using namespace llvm;

namespace {

/// A module with a memory-touching callee, so the instrumentation has real
/// cells to shadow and sea-dsa has non-trivial graphs to build.
const char *kIR = R"LLVM(
target datalayout = "e-m:o-i64:64-i128:128-n32:64-S128"

define internal i32 @callee(ptr %p) {
entry:
  %v = load i32, ptr %p
  %w = add i32 %v, 1
  store i32 %w, ptr %p
  ret i32 %w
}

define i32 @main() {
entry:
  %a = alloca i32
  %h = call noalias ptr @malloc(i64 4)
  store i32 42, ptr %a
  store ptr %a, ptr %h
  %r = call i32 @callee(ptr %a)
  ret i32 %r
}

declare noalias ptr @malloc(i64)
)LLVM";

std::unique_ptr<Module> parseIR(LLVMContext &Ctx) {
  SMDiagnostic Err;
  auto M = parseAssemblyString(kIR, Err, Ctx);
  REQUIRE_MESSAGE(M != nullptr, "test IR failed to parse");
  return M;
}

/// Control analysis owned by the test. It has the ordinary invalidate()
/// semantics, so it must not survive a pass that preserves only
/// DsaInfoAnalysis. Without it, a test that only checks DsaInfoAnalysis
/// would also pass if the fix over-corrected to PreservedAnalyses::all().
class CanaryAnalysis : public AnalysisInfoMixin<CanaryAnalysis> {
  friend AnalysisInfoMixin<CanaryAnalysis>;
  static AnalysisKey Key;

public:
  struct Result {
    bool invalidate(Module &, const PreservedAnalyses &PA,
                    ModuleAnalysisManager::Invalidator &) {
      auto PAC = PA.getChecker<CanaryAnalysis>();
      return !(PAC.preserved() || PAC.preservedSet<AllAnalysesOn<Module>>());
    }
  };

  Result run(Module &, ModuleAnalysisManager &) { return Result(); }
};
AnalysisKey CanaryAnalysis::Key;

/// The standard new-PM stack plus sea-dsa's module analyses. Member order is
/// the LLVM-documented one: MAM is destroyed before the managers it proxies.
struct PmStack {
  LoopAnalysisManager LAM;
  FunctionAnalysisManager FAM;
  CGSCCAnalysisManager CGAM;
  ModuleAnalysisManager MAM;
  PassBuilder PB;

  PmStack() {
    PB.registerModuleAnalyses(MAM);
    PB.registerCGSCCAnalyses(CGAM);
    PB.registerFunctionAnalyses(FAM);
    PB.registerLoopAnalyses(LAM);
    PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);

    MAM.registerPass([] { return seadsa::AllocWrapInfoAnalysis(); });
    MAM.registerPass([] { return seadsa::DsaLibFuncInfoAnalysis(); });
    MAM.registerPass([] { return seadsa::AllocSiteInfoAnalysis(); });
    MAM.registerPass([] { return seadsa::DsaInfoAnalysis(); });
    MAM.registerPass([] { return CanaryAnalysis(); });
  }
};

/// A pass that claims to preserve nothing, to show the checks below actually
/// detect a dropped DsaInfoAnalysis (i.e. that they would have failed before
/// the fix).
struct InvalidateEverythingPass
    : public PassInfoMixin<InvalidateEverythingPass> {
  PreservedAnalyses run(Module &, ModuleAnalysisManager &) {
    return PreservedAnalyses::none();
  }
};

} // namespace

TEST_CASE("ShadowMemNewPm.KeepsDsaInfoAnalysisCached") {
  LLVMContext Ctx;
  auto M = parseIR(Ctx);
  PmStack PM;

  std::unique_ptr<seadsa::ShadowMem> Kept;
  ModulePassManager MPM;
  MPM.addPass(seadsa::ShadowMemNewPmPass(&Kept));
  MPM.run(*M, PM.MAM);

  REQUIRE(Kept != nullptr);
  // The ModuleAnalysisManager still holds the result: it was not freed out
  // from under the GlobalAnalysis reference inside `Kept`.
  auto *Cached = PM.MAM.getCachedResult<seadsa::DsaInfoAnalysis>(*M);
  REQUIRE(Cached != nullptr);
  // ... and it is the very same object the kept ShadowMem points at, not a
  // recomputation over the already-instrumented module.
  CHECK(&Kept->getDsaAnalysis() == &Cached->getGlobalAnalysis());
}

TEST_CASE("ShadowMemNewPm.InvalidatesEverythingElse") {
  LLVMContext Ctx;
  auto M = parseIR(Ctx);
  PmStack PM;

  PM.MAM.getResult<CanaryAnalysis>(*M);
  REQUIRE(PM.MAM.getCachedResult<CanaryAnalysis>(*M) != nullptr);

  std::unique_ptr<seadsa::ShadowMem> Kept;
  ModulePassManager MPM;
  MPM.addPass(seadsa::ShadowMemNewPmPass(&Kept));
  MPM.run(*M, PM.MAM);

  // Preservation is narrow: only DsaInfoAnalysis is exempt.
  CHECK(PM.MAM.getCachedResult<CanaryAnalysis>(*M) == nullptr);
  CHECK(PM.MAM.getCachedResult<seadsa::DsaInfoAnalysis>(*M) != nullptr);
}

TEST_CASE("ShadowMemNewPm.KeptShadowMemStaysUsable") {
  LLVMContext Ctx;
  auto M = parseIR(Ctx);
  PmStack PM;

  std::unique_ptr<seadsa::ShadowMem> Kept;
  ModulePassManager MPM;
  MPM.addPass(seadsa::ShadowMemNewPmPass(&Kept));
  MPM.run(*M, PM.MAM);
  REQUIRE(Kept != nullptr);

  // What a real consumer does after the pass (SeaHorn's
  // SeaDsaHeapAbstraction walks the graphs this way). Under the old
  // PreservedAnalyses::none() this reads freed memory.
  seadsa::GlobalAnalysis &Dsa = Kept->getDsaAnalysis();
  unsigned FnsWithGraphs = 0;
  for (Function &F : *M) {
    if (F.isDeclaration()) continue;
    if (!Dsa.hasGraph(F)) continue;
    ++FnsWithGraphs;
    const seadsa::Graph &G = Dsa.getGraph(F);
    // Touch the graph so the read is not optimized away, and sanity-check
    // that it is a real pre-instrumentation graph rather than an empty shell.
    CHECK(std::distance(G.begin(), G.end()) > 0);
  }
  CHECK(FnsWithGraphs > 0);
}

TEST_CASE("ShadowMemNewPm.ControlUnpreservedPassDropsDsaInfo") {
  LLVMContext Ctx;
  auto M = parseIR(Ctx);
  PmStack PM;

  PM.MAM.getResult<seadsa::DsaInfoAnalysis>(*M);
  REQUIRE(PM.MAM.getCachedResult<seadsa::DsaInfoAnalysis>(*M) != nullptr);

  ModulePassManager MPM;
  MPM.addPass(InvalidateEverythingPass());
  MPM.run(*M, PM.MAM);

  // Confirms the assertions above are meaningful: DsaInfoAnalysis really is
  // dropped by a pass that does not preserve it.
  CHECK(PM.MAM.getCachedResult<seadsa::DsaInfoAnalysis>(*M) == nullptr);
}
