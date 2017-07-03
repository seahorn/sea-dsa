#ifndef __DSA_BOTTOM_UP_HH_
#define __DSA_BOTTOM_UP_HH_

#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/Pass.h"
#include "llvm/ADT/DenseMap.h"

#include "sea_dsa/Graph.hh"
#include "sea_dsa/CallSite.hh"
#include "sea_dsa/Mapper.hh"

namespace llvm
{
  class DataLayout;
  class TargetLibraryInfo;
  class CallGraph;
}

namespace sea_dsa
{

  class BottomUpAnalysis {

  public:
    
    typedef std::shared_ptr<Graph> GraphRef;
    typedef llvm::DenseMap<const llvm::Function *, GraphRef> GraphMap;
    
  private:
    
    typedef std::shared_ptr<SimulationMapper> SimulationMapperRef;
    typedef boost::container::flat_map<const llvm::Instruction*,
				       SimulationMapperRef> CalleeCallerMapping;
    
    const llvm::DataLayout &m_dl;
    const llvm::TargetLibraryInfo &m_tli;
    llvm::CallGraph &m_cg;
    CalleeCallerMapping m_callee_caller_map;
    
    // sanity check
    bool checkAllNodesAreMapped (const llvm::Function &callee,
				 Graph &calleeG,
				 const SimulationMapper &sm);
    
  public:
    
    static bool computeCalleeCallerMapping (const DsaCallSite &cs, 
					    Graph &calleeG, Graph &callerG, 
					    const bool onlyModified,
					    SimulationMapper &simMap); 
    
    static void cloneAndResolveArguments (const DsaCallSite &CS, 
					  Graph& calleeG, Graph& callerG);
    
    BottomUpAnalysis (const llvm::DataLayout &dl,
		      const llvm::TargetLibraryInfo &tli,
		      llvm::CallGraph &cg) 
      : m_dl(dl), m_tli(tli), m_cg(cg) {}
    
    bool runOnModule (llvm::Module &M, GraphMap &graphs);
    
    typedef typename CalleeCallerMapping::const_iterator callee_caller_mapping_const_iterator;
    
    callee_caller_mapping_const_iterator callee_caller_mapping_begin () const 
    { return m_callee_caller_map.begin(); }
    
    callee_caller_mapping_const_iterator callee_caller_mapping_end () const 
    { return m_callee_caller_map.end(); }
  };
  
  class BottomUp : public llvm::ModulePass
  {
    typedef typename BottomUpAnalysis::GraphRef GraphRef;
    typedef typename BottomUpAnalysis::GraphMap GraphMap;
    
    Graph::SetFactory m_setFactory;
    const llvm::DataLayout *m_dl;
    const llvm::TargetLibraryInfo *m_tli;
    GraphMap m_graphs;
    
  public:
    
    static char ID;
    
    BottomUp ();
    
    void getAnalysisUsage (llvm::AnalysisUsage &AU) const override;
    
    bool runOnModule (llvm::Module &M) override;
    
    const char * getPassName() const override 
    { return "BottomUp DSA pass"; }
    
    Graph& getGraph (const llvm::Function &F) const;
    bool hasGraph (const llvm::Function &F) const;
    
  };
}
#endif 
