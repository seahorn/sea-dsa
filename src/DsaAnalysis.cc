#include "llvm/IR/DataLayout.h"
#include "llvm/Target/TargetLibraryInfo.h"
#include "llvm/Analysis/MemoryBuiltins.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CommandLine.h"

#include "sea_dsa/Info.hh"
#include "sea_dsa/Global.hh"
#include "sea_dsa/DsaAnalysis.hh"

using namespace sea_dsa;
using namespace llvm;

static llvm::cl::opt<bool>
DsaCsGlobalAnalysis ("sea-dsa-cs-global",
       llvm::cl::desc ("DSA: context-sensitive analysis if enabled, else context-insensitive"),
       llvm::cl::init (true));


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

  if (DsaCsGlobalAnalysis)
    m_ga.reset (new ContextSensitiveGlobalAnalysis (*m_dl, *m_tli, cg, m_setFactory));
  else 
    m_ga.reset (new ContextInsensitiveGlobalAnalysis (*m_dl, *m_tli, cg, m_setFactory));
  
  m_ga->runOnModule (M);
  return false;
}


char DsaAnalysis::ID = 0;

static llvm::RegisterPass<DsaAnalysis> 
X ("sea-dsa", "Entry point for all SeaHorn Dsa clients");
