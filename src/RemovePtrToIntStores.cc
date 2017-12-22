#include "sea_dsa/support/RemovePtrToIntStores.hh"

#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace sea_dsa {

llvm::ModulePass *CreateRemovePtrToIntStoresPass() {
  return new RemovePtrToIntStores();
}

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

  F.dump();

  DominatorTree DT(F);

  bool Changed = false;

  auto &Ctx = F.getParent()->getContext();
  auto &TLI = getAnalysis<TargetLibraryInfoWrapperPass>().getTLI();
  auto &DL = F.getParent()->getDataLayout();

  for (auto &BB : F) {

    bool RemovedOne;
    do {
      RemovedOne = false;

      for (auto &I : BB) {
        auto *SI = dyn_cast<StoreInst>(&I);
        if (!SI)
          continue;

        auto *P2I = dyn_cast<PtrToIntInst>(SI->getOperand(0));
        if (!P2I)
          continue;

        auto *BC = dyn_cast<BitCastInst>(SI->getOperand(1));
        if (!BC)
          continue;

        auto *PtrVal = P2I->getOperand(0);
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

//        SI->dump();
//
//        dbgs() << "\nPtr2Int: ";
//        P2I->dump();
//        dbgs() << "\nBC: ";
//        BC->dump();
//        dbgs() << "\nPtrVal: ";
//        PtrVal->dump();
//        dbgs() << "\nBCVal: ";
//        BCVal->dump();

        IRBuilder<> IRB(SI);
        auto *NewBC = IRB.CreateBitCast(PtrVal, NewStoreValTy);
//        dbgs() << "\nNewBC: ";
//        NewBC->dump();
        auto *NewSI = IRB.CreateStore(NewBC, BCVal);
        (void)NewSI;
//        dbgs() << "\nNewSI: ";
//        NewSI->dump();

        SI->eraseFromParent();
        if (P2I->getNumUses() == 0)
          P2I->eraseFromParent();
        if (BC->getNumUses() == 0)
          BC->eraseFromParent();

        RemovedOne = true;
        Changed = true;
        break;
      }

    } while (RemovedOne);
  }

  return Changed;
}

void RemovePtrToIntStores::getAnalysisUsage(llvm::AnalysisUsage &AU) const {
  AU.addRequired<llvm::TargetLibraryInfoWrapperPass>();
  AU.setPreservesAll();
}

} // namespace sea_dsa

static llvm::RegisterPass<sea_dsa::RemovePtrToIntStores>
    X("sea-dsa-remove-ptrtoint-stores", "Removes ptrtoint stores");
