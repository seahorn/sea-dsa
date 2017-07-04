#include "llvm/IR/DataLayout.h"
#include "llvm/Target/TargetLibraryInfo.h"
#include "llvm/Analysis/MemoryBuiltins.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CommandLine.h"

#include "sea_dsa/Info.hh"
#include "sea_dsa/Global.hh"
#include "sea_dsa/DsaAnalysis.hh"
#include "sea_dsa/Stats.hh"

using namespace sea_dsa;
using namespace llvm;

static llvm::cl::opt<sea_dsa::GlobalAnalysisKind>
DsaGlobalAnalysis ("sea-dsa",
       llvm::cl::desc ("DSA: kind of Dsa analysis"),
       llvm::cl::values 
       (clEnumValN (CONTEXT_SENSITIVE  , "cs"   , "Context-sensitive (default)"),
        clEnumValN (CONTEXT_INSENSITIVE, "ci"   , "Context-insensitive"),
        clEnumValN (FLAT_MEMORY        , "flat" , "Flat memory"),		
	clEnumValEnd),
       llvm::cl::init (CONTEXT_SENSITIVE));

static llvm::cl::opt<bool>
DsaStats("sea-dsa-stats",
	llvm::cl::desc("Print stats about Dsa analysis"),
	llvm::cl::init (false));

		     
void DsaAnalysis::getAnalysisUsage (AnalysisUsage &AU) const {
  AU.addRequired<DataLayoutPass> ();
  AU.addRequired<TargetLibraryInfo> ();
  AU.addRequired<CallGraphWrapperPass> ();
  AU.setPreservesAll ();
}

const DataLayout& DsaAnalysis::getDataLayout () {
  assert (m_dl);
  return *m_dl;
}

const TargetLibraryInfo& DsaAnalysis::getTLI () {
  assert (m_tli);
  return *m_tli;
}

GlobalAnalysis& DsaAnalysis::getDsaAnalysis () {
  assert (m_ga);
  return *m_ga;
}

bool DsaAnalysis::runOnModule (Module &M) {
  m_dl  = &getAnalysis<DataLayoutPass>().getDataLayout ();
  m_tli = &getAnalysis<TargetLibraryInfo> ();
  auto &cg = getAnalysis<CallGraphWrapperPass> ().getCallGraph ();

  switch (DsaGlobalAnalysis) {
  case CONTEXT_INSENSITIVE:
    m_ga.reset (new ContextInsensitiveGlobalAnalysis
		(*m_dl, *m_tli, cg, m_setFactory, false));
    break;
  case FLAT_MEMORY:
    m_ga.reset (new ContextInsensitiveGlobalAnalysis
		(*m_dl, *m_tli, cg, m_setFactory, true /* use flat*/));
    break;
  default: /* CONTEXT_SENSITIVE */    
    m_ga.reset (new ContextSensitiveGlobalAnalysis
		(*m_dl, *m_tli, cg, m_setFactory));
  }
  
  m_ga->runOnModule (M);

  if (DsaStats) {
    DsaInfo i (*m_dl, *m_tli,getDsaAnalysis ());
    i.runOnModule (M);
    DsaPrintStats p (i);
    p.runOnModule (M);
  }
    
  return false;
}


char DsaAnalysis::ID = 0;

static llvm::RegisterPass<DsaAnalysis> 
X ("sea-dsa", "Entry point for all SeaHorn Dsa clients");
