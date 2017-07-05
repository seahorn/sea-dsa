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

namespace sea_dsa  {
  class DsaInfo;
}

namespace sea_dsa {

  class DsaPrintStats  {
    
    const sea_dsa::DsaInfo &m_dsa;
    
  public:
    
    DsaPrintStats (const sea_dsa::DsaInfo& dsa): m_dsa (dsa) {}
    
    void runOnModule (llvm::Module &M);

  };
  
}
#endif 
