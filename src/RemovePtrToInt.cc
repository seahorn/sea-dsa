#include "sea_dsa/support/RemovePtrToInt.hh"

#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/PassManager.h"

#include "sea_dsa/support/Debug.h"

#define RPTI_LOG(...) LOG("remove-ptrtoint", __VA_ARGS__)

using namespace llvm;

namespace sea_dsa {

char RemovePtrToInt::ID = 0;

static bool visitStoreInst(StoreInst *SI, Function &F, const DataLayout &DL,
                           SmallPtrSetImpl<StoreInst *> &StoresToErase,
                           SmallPtrSetImpl<Instruction *> &MaybeUnusedInsts) {
  assert(SI);

  auto *P2I = dyn_cast<PtrToIntInst>(SI->getValueOperand());
  if (!P2I)
    return false;

  auto *BC = dyn_cast<BitCastInst>(SI->getPointerOperand());
  if (!BC)
    return false;

  auto *PtrVal = P2I->getPointerOperand();
  auto *PtrIntTy = DL.getIntPtrType(PtrVal->getType());
  if (PtrIntTy != P2I->getType())
    return false;

  auto *BCTy = BC->getType();
  if (!BCTy->isPointerTy() || BCTy->getPointerElementType() != PtrIntTy)
    return false;

  auto *BCVal = BC->getOperand(0);
  auto *NewStoreDestTy = BCVal->getType();
  auto *NewStoreValTy = NewStoreDestTy->getPointerElementType();
  if (!NewStoreValTy->isPointerTy())
    return false;

  IRBuilder<> IRB(SI);
  auto *NewBC = IRB.CreateBitCast(PtrVal, NewStoreValTy);
  IRB.CreateStore(NewBC, BCVal);

  // Collect instructions to erase. Deffer that not to invalidate iterators.
  StoresToErase.insert(SI);
  MaybeUnusedInsts.insert(P2I);
  MaybeUnusedInsts.insert(BC);

  return true;
}

static bool
visitIntToPtrInst(IntToPtrInst *I2P, Function &F, const DataLayout &DL,
                  DominatorTree &DT,
                  SmallDenseMap<PHINode *, PHINode *> &NewPhis,
                  SmallPtrSetImpl<Instruction *> &MaybeUnusedInsts) {
  assert(I2P);

  auto *IntVal = I2P->getOperand(0);
  auto *PN = dyn_cast<PHINode>(IntVal);
  if (!PN)
    return false;

  RPTI_LOG(errs() << F.getName() << ":\n"; I2P->print(errs());
           PN->print(errs() << "\n"); errs() << "\n");

  if (NewPhis.count(PN) > 0) {
    IRBuilder<> IRB(I2P);
    auto *NewPhi = NewPhis[PN];
    I2P->replaceAllUsesWith(IRB.CreateBitCast(NewPhi, I2P->getType()));
    MaybeUnusedInsts.insert(I2P);

    RPTI_LOG(errs() << "\n!!!! Reused new PHI ~~~~~ \n");
    return true;
  }

  if (!llvm::all_of(PN->incoming_values(),
                    [](Value *IVal) { return isa<PtrToIntInst>(IVal); })) {
    RPTI_LOG(
        errs() << "Not all incoming values are PtrToInstInsts, skipping PHI\n");
    return false;
  }

  // All confirmed to come from PtrToInt, so we can rewrite the PhiNode.
  IRBuilder<> IRB(PN);
  auto *NewPN = IRB.CreatePHI(I2P->getType(), PN->getNumIncomingValues(),
                              PN->getName() + ".rpti");

  for (unsigned i = 0; i < PN->getNumIncomingValues(); ++i) {
    auto *IBB = PN->getIncomingBlock(i);
    Value *IVal = PN->getIncomingValue(i);

    if (auto *P2I = dyn_cast<PtrToIntInst>(IVal)) {
      assert(DT.dominates(P2I, I2P));

      auto *Ptr = P2I->getPointerOperand();
      assert(Ptr);

      IRB.SetInsertPoint(P2I);
      NewPN->addIncoming(IRB.CreateBitCast(Ptr, I2P->getType()), IBB);
      RPTI_LOG(NewPN->print(errs(), 1));

      MaybeUnusedInsts.insert(P2I);
    }
    // It's better teach instcombine not to generate loads we see here.
    //    else if (auto *LI = dyn_cast<LoadInst>(IVal)) {
    //      assert(DT.dominates(LI, I2P));
    //
    //      auto *Loaded = LI->getPointerOperand();
    //      Loaded->dump();
    //
    //      IRB.SetInsertPoint(LI);
    //
    //      auto *CastedLoaded = IRB.CreateBitCast(Loaded,
    //      I2P->getType()->getPointerTo(0), Loaded->getName() + ".rpti"); auto
    //      *NewLI = IRB.CreateLoad(CastedLoaded, LI->getName() + ".rpti");
    //
    //      NewPN->addIncoming(NewLI, IBB);
    //      NewLI->dump();
    //      NewPN->dump();
    //
    //      MaybeUnusedInsts.insert(LI);
    //
    //      if (LI->getName().find("__x.sroa.0.0.") != size_t(-1)) {
    //        //__asm__("int3");
    //        //assert(false);
    //      }
    //    }
  }

  NewPhis[PN] = NewPN;
  MaybeUnusedInsts.insert(PN);

  I2P->replaceAllUsesWith(NewPN);
  MaybeUnusedInsts.insert(I2P);

  return false;
}

bool RemovePtrToInt::runOnFunction(Function &F) {
  if (F.isDeclaration())
    return false;

  // Skip special functions
  if (F.getName().startswith("seahorn.") || F.getName().startswith("shadow.") ||
      F.getName().startswith("verifier."))
    return false;

  RPTI_LOG(errs() << "\n~~~~~~~ Start of RP2I on " << F.getName()
                  << " ~~~~~ \n");

  bool Changed = false;
  auto &DL = F.getParent()->getDataLayout();
  // Would be better to get the DominatorTreeWrapperPass, but the pass manager
  // crashed when it's required.
  auto &DT = getAnalysis<DominatorTreeWrapperPass>().getDomTree();
  SmallPtrSet<StoreInst *, 8> StoresToErase;
  SmallPtrSet<Instruction *, 16> MaybeUnusedInsts;
  SmallDenseMap<PHINode *, PHINode *> NewPhis;

  for (auto &BB : F) {
    for (auto &I : BB) {
      if (auto *SI = dyn_cast<StoreInst>(&I)) {
        Changed |= visitStoreInst(SI, F, DL, StoresToErase, MaybeUnusedInsts);
        continue;
      }

      if (auto *I2P = dyn_cast<IntToPtrInst>(&I)) {
        Changed |= visitIntToPtrInst(I2P, F, DL, DT, NewPhis, MaybeUnusedInsts);
        continue;
      }
    }
  }

  for (auto *SI : StoresToErase)
    SI->eraseFromParent();

  // Instructions to erase may depend on one another. Sort them and delete in
  // 'deterministic'.
  DT.updateDFSNumbers();
  SmallVector<Instruction *, 16> OrderedMaybeUnused(MaybeUnusedInsts.begin(),
                                                    MaybeUnusedInsts.end());
  std::sort(OrderedMaybeUnused.begin(), OrderedMaybeUnused.end(),
            [&DT](Instruction *First, Instruction *Second) {
              auto *FirstDTNode = DT.getNode(First->getParent());
              const unsigned FirstDFSNum =
                  FirstDTNode ? FirstDTNode->getDFSNumIn() : 0;

              auto *SecondDTNode = DT.getNode(Second->getParent());
              const unsigned SecondDFSNum =
                  SecondDTNode ? SecondDTNode->getDFSNumIn() : 0;

              return std::make_tuple(FirstDFSNum, First->getNumUses(), First) >
                     std::make_tuple(SecondDFSNum, Second->getNumUses(),
                                     Second);
            });

  for (auto *I : OrderedMaybeUnused)
    if (I->getNumUses() == 0) {
      RPTI_LOG(errs() << "\terasing: " << I->getName() << "\n");
      I->eraseFromParent();
    } else {
      RPTI_LOG(errs() << "\t_NOT_ erasing: " << I->getName() << "\n");
    }

  RPTI_LOG(errs() << "\n~~~~~~~ End of RP2I on " << F.getName() << " ~~~~~ \n";
           errs().flush());

  return Changed;
}

void RemovePtrToInt::getAnalysisUsage(llvm::AnalysisUsage &AU) const {
  AU.setPreservesCFG();
  AU.addRequired<llvm::DominatorTreeWrapperPass>();
  AU.addRequired<llvm::TargetLibraryInfoWrapperPass>();
}

} // namespace sea_dsa

static llvm::RegisterPass<sea_dsa::RemovePtrToInt>
X("seadsa-remove-ptrtoint",
  "Removes ptrtoint");
  
