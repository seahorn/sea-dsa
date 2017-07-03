#ifndef __DSA_ANALYSIS_HH_
#define __DSA_ANALYSIS_HH_

#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/Pass.h"

#include "sea_dsa/Graph.hh"
#include "sea_dsa/Global.hh"
#include "sea_dsa/Info.hh"

/* Entry point for all Dsa clients */

namespace llvm  {
    class DataLayout;
    class TargetLibraryInfo;
    class CallGraph;
    class Value;
}

namespace sea_dsa {
  
  class DsaAnalysis : public llvm::ModulePass {

    const llvm::DataLayout *m_dl;
    llvm::TargetLibraryInfo *m_tli;
    Graph::SetFactory m_setFactory;
    std::unique_ptr<GlobalAnalysis> m_ga;
    
  public:
      
    static char ID;       
    
    DsaAnalysis ():
      ModulePass (ID), m_dl(nullptr), m_tli(nullptr), m_ga(nullptr) { }
    
    void getAnalysisUsage (llvm::AnalysisUsage &AU) const override;
    
    bool runOnModule (llvm::Module &M) override;
    
    const char * getPassName() const 
    { return "SeaHorn Dsa analysis: entry point for all Dsa clients"; }

    const llvm::DataLayout& getDataLayout ();

    const llvm::TargetLibraryInfo& getTLI ();
    
    GlobalAnalysis& getDsaAnalysis ();
  };
}

namespace sea_dsa {
  llvm::Pass *createDsaInfoPass ();      
  llvm::Pass *createDsaPrintStatsPass ();  
  llvm::Pass *createDsaPrinterPass ();
  llvm::Pass *createDsaViewerPass ();
}

#endif 
