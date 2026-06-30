#ifndef SEADSA_REMOVE_PTR_TO_INT
#define SEADSA_REMOVE_PTR_TO_INT

#include "llvm/IR/PassManager.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"

namespace seadsa {

class RemovePtrToInt: public llvm::FunctionPass {
public:
  static char ID;

  RemovePtrToInt() : llvm::FunctionPass(ID) {}

  bool runOnFunction(llvm::Function &F) override;
  bool runImpl(llvm::Function &F, llvm::DominatorTree &DT);
  void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;

  virtual llvm::StringRef getPassName() const override {
    return "sea-dsa: remove ptrtoint";
  }
};

llvm::Pass* createRemovePtrToIntPass();

class RemovePtrToIntPass : public llvm::PassInfoMixin<RemovePtrToIntPass> {
public:
  llvm::PreservedAnalyses run(llvm::Function &F, llvm::FunctionAnalysisManager &FAM);
};
} // namespace seadsa

#endif
