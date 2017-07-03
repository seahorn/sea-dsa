#ifndef __DSA_LOCAL_HH_
#define __DSA_LOCAL_HH_

#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/Pass.h"

#include "llvm/ADT/DenseSet.h"

#include "sea_dsa/Graph.hh"

namespace llvm 
{
   class DataLayout;
   class TargetLibraryInfo;
}

namespace sea_dsa {
  
  class LocalAnalysis
  {
    const llvm::DataLayout &m_dl;
    const llvm::TargetLibraryInfo &m_tli;
    
  public:
    LocalAnalysis (const llvm::DataLayout &dl,
		   const llvm::TargetLibraryInfo &tli) :
      m_dl(dl), m_tli(tli) {}
    
    void runOnFunction (llvm::Function &F, Graph &g);
    
  };
  
  class Local : public llvm::ModulePass
  {
    Graph::SetFactory m_setFactory;
    typedef std::shared_ptr<Graph> GraphRef;
    llvm::DenseMap<const llvm::Function*, GraphRef> m_graphs;
    
    const llvm::DataLayout *m_dl;
    const llvm::TargetLibraryInfo *m_tli;
    
  public:
    static char ID;
    
    Local ();
    
    void getAnalysisUsage (llvm::AnalysisUsage &AU) const override;
    
    bool runOnModule (llvm::Module &M) ;
    
    bool runOnFunction (llvm::Function &F);
    
    const char * getPassName() const 
    { return "Dsa local pass"; }
    
    bool hasGraph(const llvm::Function& F) const;
    
    const Graph& getGraph(const llvm::Function& F) const;
  };
}
#endif 
