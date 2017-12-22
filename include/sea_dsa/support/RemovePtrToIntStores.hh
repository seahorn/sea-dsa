#ifndef SEA_DSA_REMOVE_PTR_TO_INT_STORES
#define SEA_DSA_REMOVE_PTR_TO_INT_STORES

#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"

namespace sea_dsa {

struct RemovePtrToIntStores : public llvm::ModulePass {
  static char ID;

  RemovePtrToIntStores() : llvm::ModulePass(ID) {}

  bool runOnModule(llvm::Module &M);
  bool runOnFunction(llvm::Function &F);
  void getAnalysisUsage(llvm::AnalysisUsage &AU) const;

  virtual const char *getPassName() const {
    return "sea-dsa: remove ptrtoint stores";
  }
};

llvm::ModulePass *CreateRemovePtrToIntStoresPass();

} // namespace sea_dsa

#endif
