#include "sea_dsa/DsaAnalysis.hh"

#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/MemoryBuiltins.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

#include "sea_dsa/AllocWrapInfo.hh"
#include "sea_dsa/Global.hh"
#include "sea_dsa/Info.hh"
#include "sea_dsa/Stats.hh"
#include "sea_dsa/support/Brunch.hh"
#include "sea_dsa/support/RemovePtrToInt.hh"

namespace sea_dsa {
extern bool IsTypeAware;
}

using namespace sea_dsa;
using namespace llvm;

static llvm::cl::opt<sea_dsa::GlobalAnalysisKind> DsaGlobalAnalysis(
    "sea-dsa", llvm::cl::desc("DSA: kind of Dsa analysis"),
    llvm::cl::values(
        clEnumValN(CONTEXT_SENSITIVE, "cs",
                   "Context-sensitive as in SAS'17 (default)"),
        clEnumValN(BUTD_CONTEXT_SENSITIVE, "butd-cs", "Bottom-up + top-down"),
        clEnumValN(CONTEXT_INSENSITIVE, "ci", "Context-insensitive"),
        clEnumValN(FLAT_MEMORY, "flat", "Flat memory")),
    llvm::cl::init(CONTEXT_SENSITIVE));

static llvm::cl::opt<bool>
    DsaStats("sea-dsa-stats", llvm::cl::desc("Print stats about Dsa analysis"),
             llvm::cl::init(false));

void DsaAnalysis::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<RemovePtrToInt>();
  AU.addRequired<CallGraphWrapperPass>();
  AU.addRequired<TargetLibraryInfoWrapperPass>();
  AU.addRequired<AllocWrapInfo>();
  AU.setPreservesAll();
}

const DataLayout &DsaAnalysis::getDataLayout() {
  assert(m_dl);
  return *m_dl;
}

const TargetLibraryInfo &DsaAnalysis::getTLI() {
  assert(m_tli);
  return *m_tli;
}

GlobalAnalysis &DsaAnalysis::getDsaAnalysis() {
  assert(m_ga);
  return *m_ga;
}

bool DsaAnalysis::runOnModule(Module &M) {
  m_dl = &M.getDataLayout();
  m_tli = &getAnalysis<TargetLibraryInfoWrapperPass>().getTLI();
  m_allocInfo = &getAnalysis<AllocWrapInfo>();
  auto &cg = getAnalysis<CallGraphWrapperPass>().getCallGraph();

  BrunchTimer dsaTime("DSA_TOTAL");

  switch (DsaGlobalAnalysis) {
  case CONTEXT_INSENSITIVE:
    m_ga.reset(new ContextInsensitiveGlobalAnalysis(*m_dl, *m_tli, *m_allocInfo,
                                                    cg, m_setFactory, false));
    break;
  case FLAT_MEMORY:
    m_ga.reset(new ContextInsensitiveGlobalAnalysis(
        *m_dl, *m_tli, *m_allocInfo, cg, m_setFactory, true /* use flat*/));
    break;
  case BUTD_CONTEXT_SENSITIVE:
    m_ga.reset(new BottomUpTopDownGlobalAnalysis(*m_dl, *m_tli, *m_allocInfo,
                                                 cg, m_setFactory));
    break;
  default: /* CONTEXT_SENSITIVE */
    m_ga.reset(new ContextSensitiveGlobalAnalysis(*m_dl, *m_tli, *m_allocInfo,
                                                  cg, m_setFactory));
  }

  m_ga->runOnModule(M);
  dsaTime.stop();
  SEA_DSA_BRUNCH_STAT("SEA_DSA_TYPE_AWARE", sea_dsa::IsTypeAware ? 1 : 0);
  SEA_DSA_BRUNCH_STAT("SEA_DSA_RSS_KB", sea_dsa::GetCurrentMemoryUsageKb());

  if (DsaStats) {
    DsaInfo i(*m_dl, *m_tli, getDsaAnalysis());
    i.runOnModule(M);
    DsaPrintStats p(i);
    p.runOnModule(M);
  }

  return false;
}

char DsaAnalysis::ID = 0;

static llvm::RegisterPass<DsaAnalysis>
    X("seadsa", "Entry point for all SeaHorn Dsa clients");
