#include "seadsa/DsaAnalysis.hh"

#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/MemoryBuiltins.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

#include "seadsa/AllocWrapInfo.hh"
#include "seadsa/DsaLibFuncInfo.hh"
#include "seadsa/Global.hh"
#include "seadsa/Info.hh"
#include "seadsa/InitializePasses.hh"
#include "seadsa/Stats.hh"
#include "seadsa/support/RemovePtrToInt.hh"

namespace seadsa {
extern bool g_IsTypeAware;
}

using namespace seadsa;
using namespace llvm;

static llvm::cl::opt<seadsa::GlobalAnalysisKind> DsaGlobalAnalysis(
    "sea-dsa", llvm::cl::desc("Choose the SeaDsa analysis"),
    llvm::cl::values(
        clEnumValN(
            GlobalAnalysisKind::CONTEXT_SENSITIVE, "cs",
            "Context-sensitive for VC generation as in SAS'17 (default)"),
        clEnumValN(GlobalAnalysisKind::BUTD_CONTEXT_SENSITIVE, "butd-cs",
                   "Bottom-up + top-down"),
        clEnumValN(GlobalAnalysisKind::BU, "bu", "Bottom-up"),
        clEnumValN(GlobalAnalysisKind::CONTEXT_INSENSITIVE, "ci",
                   "Context-insensitive for VC generation"),
        clEnumValN(GlobalAnalysisKind::FLAT_MEMORY, "flat", "Flat memory")),
    llvm::cl::init(GlobalAnalysisKind::CONTEXT_SENSITIVE));

namespace seadsa {
bool PrintDsaStats;
}

static llvm::cl::opt<bool, true>
    XDsaStats("sea-dsa-stats",
              llvm::cl::desc("Print stats about SeaDsa analysis"),
              llvm::cl::location(seadsa::PrintDsaStats), llvm::cl::init(false));

void DsaAnalysis::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<RemovePtrToInt>();
  AU.addRequired<CallGraphWrapperPass>();
  AU.addRequired<TargetLibraryInfoWrapperPass>();
  // dependency for AllocWrapInfo
  AU.addRequired<LoopInfoWrapperPass>();
  AU.addRequired<AllocWrapInfo>();
  AU.addRequired<DsaLibFuncInfo>();
  AU.setPreservesAll();
}

const DataLayout &DsaAnalysis::getDataLayout() {
  assert(m_dl);
  return *m_dl;
}

const TargetLibraryInfo &DsaAnalysis::getTLI(const Function &F) {
  assert(m_tliWrapper);
  return m_tliWrapper->getTLI(F);
}

GlobalAnalysis &DsaAnalysis::getDsaAnalysis() {
  assert(m_ga);
  return *m_ga;
}

static std::unique_ptr<GlobalAnalysis>
mkGlobalAnalysis(const DataLayout &dl, TargetLibraryInfoWrapperPass &tli,
                 const AllocWrapInfo &awi, const DsaLibFuncInfo &dlfi,
                 CallGraph &cg, Graph::SetFactory &sf) {
  switch (DsaGlobalAnalysis) {
  case GlobalAnalysisKind::CONTEXT_INSENSITIVE:
    return std::make_unique<ContextInsensitiveGlobalAnalysis>(
        dl, tli, awi, dlfi, cg, sf, false);
  case GlobalAnalysisKind::FLAT_MEMORY:
    return std::make_unique<ContextInsensitiveGlobalAnalysis>(
        dl, tli, awi, dlfi, cg, sf, true /* use flat */);
  case GlobalAnalysisKind::BUTD_CONTEXT_SENSITIVE:
    return std::make_unique<BottomUpTopDownGlobalAnalysis>(dl, tli, awi, dlfi,
                                                           cg, sf);
  case GlobalAnalysisKind::BU:
    return std::make_unique<BottomUpGlobalAnalysis>(dl, tli, awi, dlfi, cg, sf);
  default: /* CONTEXT_SENSITIVE */
    return std::make_unique<ContextSensitiveGlobalAnalysis>(
        dl, tli, awi, dlfi, cg, sf, true /* always store summary graphs */);
  }
}

bool DsaAnalysis::runOnModule(Module &M) {
  m_dl = &M.getDataLayout();
  m_tliWrapper = &getAnalysis<TargetLibraryInfoWrapperPass>();
  m_allocInfo = &getAnalysis<AllocWrapInfo>();
  m_dsaLibFuncInfo = &getAnalysis<DsaLibFuncInfo>();
  m_allocInfo->initialize(M, this);
  m_dsaLibFuncInfo->initialize(M);
  auto &cg = getAnalysis<CallGraphWrapperPass>().getCallGraph();

  m_ga = mkGlobalAnalysis(*m_dl, *m_tliWrapper, *m_allocInfo,
                          *m_dsaLibFuncInfo, cg, m_setFactory);

  m_ga->runOnModule(M);

  if (XDsaStats || m_print_stats) {
    DsaInfo i(*m_dl, *m_tliWrapper, getDsaAnalysis());
    i.runOnModule(M);
    DsaPrintStats p(i);
    p.runOnModule(M);
  }

  return false;
}

char DsaAnalysis::ID = 0;

using namespace seadsa;
INITIALIZE_PASS_BEGIN(DsaAnalysis, "dsa-wrapper",
                      "Entry point for all SeaDsa clients", false, false)
INITIALIZE_PASS_DEPENDENCY(RemovePtrToInt)
// dependency for immutable AllowWrapInfo
INITIALIZE_PASS_DEPENDENCY(LoopInfoWrapperPass);
INITIALIZE_PASS_DEPENDENCY(AllocWrapInfo)
INITIALIZE_PASS_DEPENDENCY(CallGraphWrapperPass)
INITIALIZE_PASS_DEPENDENCY(TargetLibraryInfoWrapperPass)
INITIALIZE_PASS_DEPENDENCY(DsaLibFuncInfo)
INITIALIZE_PASS_END(DsaAnalysis, "dsa-wrapper",
                    "Entry point for all SeaDsa clients", false, false)

// --- self-contained DsaInfo for new-PM consumers ---
seadsa::LocalDsaInfo::LocalDsaInfo(Module &M,
                                   TargetLibraryInfoWrapperPass &tliWrapper) {
  const DataLayout &dl = M.getDataLayout();
  m_allocInfo = std::make_unique<AllocWrapInfo>(&tliWrapper);
  m_allocInfo->initialize(M, nullptr);
  m_dsaLibFuncInfo = std::make_unique<DsaLibFuncInfo>();
  m_dsaLibFuncInfo->initialize(M);
  m_cg = std::make_unique<CallGraph>(M);
  m_ga = mkGlobalAnalysis(dl, tliWrapper, *m_allocInfo, *m_dsaLibFuncInfo, *m_cg,
                          m_setFactory);
  m_ga->runOnModule(M);
  m_info = std::make_unique<DsaInfo>(dl, tliWrapper, *m_ga);
  m_info->runOnModule(M);
}

seadsa::LocalDsaInfo::~LocalDsaInfo() = default;
