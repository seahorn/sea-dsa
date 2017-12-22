#include "sea_dsa/support/RemovePtrToIntStores.hh"

#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/PassManager.h"

using namespace llvm;

namespace sea_dsa {

char RemovePtrToIntStores::ID = 0;

bool RemovePtrToIntStores::runOnModule(llvm::Module &M) {
  bool Changed = false;
  for (auto &F : M)
    Changed |= runOnFunction(F);

  return Changed;
}

bool RemovePtrToIntStores::runOnFunction(Function &F) {
  if (F.isDeclaration())
    return false;

  // Skip special functions
  if (F.getName().startswith("seahorn.") || F.getName().startswith("shadow.") ||
      F.getName().startswith("verifier."))
    return false;

  bool Changed = false;
  auto &DL = F.getParent()->getDataLayout();
  SmallPtrSet<StoreInst *, 8> StoresToErase;
  SmallPtrSet<Instruction *, 16> MaybeUnusedInsts;

  for (auto &BB : F) {
    for (auto &I : BB) {
      auto *SI = dyn_cast<StoreInst>(&I);
      if (!SI)
        continue;

      auto *P2I = dyn_cast<PtrToIntInst>(SI->getValueOperand());
      if (!P2I)
        continue;

      auto *BC = dyn_cast<BitCastInst>(SI->getPointerOperand());
      if (!BC)
        continue;

      auto *PtrVal = P2I->getPointerOperand();
      auto *PtrIntTy = DL.getIntPtrType(PtrVal->getType());
      if (PtrIntTy != P2I->getType())
        continue;

      auto *BCTy = BC->getType();
      if (!BCTy->isPointerTy() || BCTy->getPointerElementType() != PtrIntTy)
        continue;

      auto *BCVal = BC->getOperand(0);
      auto *NewStoreDestTy = BCVal->getType();
      auto *NewStoreValTy = NewStoreDestTy->getPointerElementType();
      if (!NewStoreValTy->isPointerTy())
        continue;

      IRBuilder<> IRB(SI);
      auto *NewBC = IRB.CreateBitCast(PtrVal, NewStoreValTy);
      IRB.CreateStore(NewBC, BCVal);

      // Collect instructions to erase. Deffer that not to invalidate iterators.
      StoresToErase.insert(SI);
      MaybeUnusedInsts.insert(P2I);
      MaybeUnusedInsts.insert(BC);

      Changed = true;
    }
  }

  for (auto *SI : StoresToErase)
    SI->eraseFromParent();

  // Erase unused instructions in any order for simplicity. They may depend on
  // one another, and the best way to do that would be to sort that by DomTree
  // DFS In/Out numbers.
  for (auto *I : MaybeUnusedInsts)
    if (I->getNumUses() == 0)
      I->eraseFromParent();

  return Changed;
}

void RemovePtrToIntStores::getAnalysisUsage(llvm::AnalysisUsage &AU) const {
  AU.addRequired<llvm::TargetLibraryInfoWrapperPass>();
  AU.setPreservesAll();
}

} // namespace sea_dsa

static llvm::RegisterPass<sea_dsa::RemovePtrToIntStores>
    X("sea-dsa-remove-ptrtoint-stores", "Removes ptrtoint stores");
