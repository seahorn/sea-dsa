#ifndef SEA_DSA_REMOVE_PTR_TO_INT
#define SEA_DSA_REMOVE_PTR_TO_INT

#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"

namespace sea_dsa {

struct RemovePtrToInt: public llvm::ModulePass {
  static char ID;

  RemovePtrToInt() : llvm::ModulePass(ID) {}

  bool runOnModule(llvm::Module &M);
  bool runOnFunction(llvm::Function &F);
  void getAnalysisUsage(llvm::AnalysisUsage &AU) const;

  virtual const char *getPassName() const {
    return "sea-dsa: remove ptrtoint";
  }
};

} // namespace sea_dsa

#endif
