#ifndef __SEA_DSA_NAME_VALUES__HPP_
#define __SEA_DSA_NAME_VALUES__HPP_

#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/ADT/StringRef.h"

namespace sea_dsa {
  struct NameValues : public llvm::ModulePass {
    static char ID;
    NameValues () : llvm::ModulePass (ID) {}
    bool runOnModule (llvm::Module &M);
    bool runOnFunction (llvm::Function &F);
    void getAnalysisUsage (llvm::AnalysisUsage &AU) const {
      AU.setPreservesAll ();
    }
    virtual llvm::StringRef getPassName () const {
      return "sea-dsa: names all unnamed values";
    }
  };
}

#endif
