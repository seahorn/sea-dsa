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
#include "sea_dsa/support/Debug.h"
#include "sea_dsa/support/RemovePtrToInt.hh"

namespace sea_dsa {
extern bool IsTypeAware;
}

using namespace sea_dsa;
using namespace llvm;

static llvm::cl::opt<sea_dsa::GlobalAnalysisKind> DsaGlobalAnalysis(
    "sea-dsa", llvm::cl::desc("Choose the SeaDsa analysis"),
    llvm::cl::values(
        clEnumValN(GlobalAnalysisKind::CONTEXT_SENSITIVE, "cs",
                   "Context-sensitive for VC generation as in SAS'17 (default)"),
        clEnumValN(GlobalAnalysisKind::BUTD_CONTEXT_SENSITIVE, "butd-cs",
		   "Bottom-up + top-down"),
	clEnumValN(GlobalAnalysisKind::BU, "bu", "Bottom-up"),
        clEnumValN(GlobalAnalysisKind::CONTEXT_INSENSITIVE, "ci",
		   "Context-insensitive for VC generation"),
        clEnumValN(GlobalAnalysisKind::FLAT_MEMORY, "flat", "Flat memory")),
    llvm::cl::init(GlobalAnalysisKind::CONTEXT_SENSITIVE));

namespace sea_dsa {
bool PrintDsaStats;
}

static llvm::cl::opt<bool, true>
XDsaStats("sea-dsa-stats", llvm::cl::desc("Print stats about SeaDsa analysis"),
	  llvm::cl::location(sea_dsa::PrintDsaStats),
	  llvm::cl::init(false));


namespace sea_dsa {
SeaDsaLogOpt loc;
}

static llvm::cl::opt<sea_dsa::SeaDsaLogOpt, true, llvm::cl::parser<std::string>> 
LogClOption ("sea-dsa-log",
             llvm::cl::desc ("Enable specified log level"),
             llvm::cl::location (sea_dsa::loc),
             llvm::cl::value_desc ("string"),
             llvm::cl::ValueRequired, llvm::cl::ZeroOrMore);

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

  switch (DsaGlobalAnalysis) {
  case GlobalAnalysisKind::CONTEXT_INSENSITIVE:
    m_ga.reset(new ContextInsensitiveGlobalAnalysis(*m_dl, *m_tli, *m_allocInfo,
                                                    cg, m_setFactory, false));
    break;
  case GlobalAnalysisKind::FLAT_MEMORY:
    m_ga.reset(new ContextInsensitiveGlobalAnalysis(
        *m_dl, *m_tli, *m_allocInfo, cg, m_setFactory, true /* use flat*/));
    break;
  case GlobalAnalysisKind::BUTD_CONTEXT_SENSITIVE:
    m_ga.reset(new BottomUpTopDownGlobalAnalysis(
        *m_dl, *m_tli, *m_allocInfo, cg, m_setFactory));
    break;
  case GlobalAnalysisKind::BU:
    m_ga.reset(new BottomUpGlobalAnalysis(*m_dl, *m_tli, *m_allocInfo, cg,
                                          m_setFactory));
    break;
  default: /* CONTEXT_SENSITIVE */
    m_ga.reset(new ContextSensitiveGlobalAnalysis(
        *m_dl, *m_tli, *m_allocInfo, cg, m_setFactory,
        true /* always store summary graphs*/));
  }

  m_ga->runOnModule(M);

  if (XDsaStats || m_print_stats) {
    DsaInfo i(*m_dl, *m_tli, getDsaAnalysis());
    i.runOnModule(M);
    DsaPrintStats p(i);
    p.runOnModule(M);
  }

  return false;
}

char DsaAnalysis::ID = 0;

static llvm::RegisterPass<DsaAnalysis>
    X("seadsa-wrapper", "Entry point for all SeaDsa clients");
