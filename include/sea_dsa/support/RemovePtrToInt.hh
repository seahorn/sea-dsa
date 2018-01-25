#ifndef SEA_DSA_REMOVE_PTR_TO_INT
#define SEA_DSA_REMOVE_PTR_TO_INT

#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"

namespace sea_dsa {

struct RemovePtrToInt: public llvm::FunctionPass {
  static char ID;

  RemovePtrToInt() : llvm::FunctionPass(ID) {}

  bool runOnFunction(llvm::Function &F) override;
  void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;

  virtual llvm::StringRef getPassName() const override {
    return "sea-dsa: remove ptrtoint";
  }
};

} // namespace sea_dsa

#endif
