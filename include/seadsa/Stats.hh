#ifndef __DSA_STATS_HH_
#define __DSA_STATS_HH_

#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/Pass.h"

/* Print statistics about dsa */

namespace llvm {
  class Value;
  class Function;
  class Module;
  class raw_ostream;
}

namespace seadsa  {
  class DsaInfo;
}

namespace seadsa {

  class DsaPrintStats  {
    
    const seadsa::DsaInfo &m_dsa;
    
  public:
    
    DsaPrintStats (const seadsa::DsaInfo& dsa): m_dsa (dsa) {}
    
    void runOnModule (llvm::Module &M);

  };
  
}
#endif 
