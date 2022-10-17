#include "seadsa/support/RemovePtrToInt.hh"

#include "seadsa/InitializePasses.hh"
#include "seadsa/support/Debug.h"

#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Transforms/Utils/Local.h"

#define DEBUG_TYPE "sea-remove-ptrtoint"

using namespace llvm;

namespace seadsa {

/**
 clang-format off

 Rewrites
  %_311 = bitcast i8** @msg_buf to i32*
  %_312 = load i32, i32* %_311, align 4
  %_313 = bitcast %struct.iovec* %_9 to i32*
  store i32 %_312, i32* %_313, align 4

  into

  %_311.ptr = bitcast i8** @msg_buf to i8**
  %_312.ptr = load i8*, i8** @_311.ptr, align 4
  %_313.ptr = bitcast %struct.iovec* %_9 to i8**
  store i8* %_312.ptr, i8** %_313.ptr, align 4

  clang-format on
 */

static bool
visitIntStoreInst(StoreInst *SI, Function &F, const DataLayout &DL,
                  SmallPtrSetImpl<StoreInst *> &StoresToErase,
                  SmallPtrSetImpl<Instruction *> &MaybeUnusedInsts) {

  auto *IntPtrTy = DL.getIntPtrType(SI->getContext());
  auto *LI = dyn_cast<LoadInst>(SI->getValueOperand());

  if (!LI) return false;
  if (LI->getType() != IntPtrTy) return false;

  auto *loadAddrCast = dyn_cast<BitCastInst>(LI->getPointerOperand());
  if (!loadAddrCast) return false;

  auto *storeAddrCast = dyn_cast<BitCastInst>(SI->getPointerOperand());
  if (!storeAddrCast) return false;

  auto *loadAddr = loadAddrCast->getOperand(0);
  auto *storeAddr = storeAddrCast->getOperand(0);

  IRBuilder<> IRB(LI);

  auto *Int8PtrPtrTy = Type::getInt8PtrTy(SI->getContext())->getPointerTo();
  auto newLI = IRB.CreateLoad(Type::getInt8PtrTy(SI->getContext()),
                              IRB.CreateBitCast(loadAddr, Int8PtrPtrTy));
  if (LI->hasName()) newLI->setName(LI->getName());
  newLI->setAlignment(LI->getAlign());
  newLI->setOrdering(LI->getOrdering());
  newLI->setSyncScopeID(LI->getSyncScopeID());

  IRB.SetInsertPoint(SI);
  auto newSI =
      IRB.CreateStore(newLI, IRB.CreateBitCast(storeAddr, Int8PtrPtrTy));
  newSI->setAlignment(SI->getAlign());
  newSI->setOrdering(SI->getOrdering());
  newSI->setSyncScopeID(SI->getSyncScopeID());
  newSI->setVolatile(SI->isVolatile());
  newSI->copyMetadata(*SI);

  DOG(llvm::errs() << "Replaced\n" << *SI << "\nby\n" << *newSI << "\n";);

  StoresToErase.insert(SI);
  MaybeUnusedInsts.insert(LI);
  MaybeUnusedInsts.insert(loadAddrCast);
  MaybeUnusedInsts.insert(storeAddrCast);
  return true;
}

/**
   clang-format off

   ;from
   %8 = ptrtoint i8* %6 to i64, !dbg !763
   store volatile i64 %8, i64* %.sroa.13, align 8, !dbg !763, !tbaa !764

   ;to
   %addr = bitcast %i64* %.sroa.13 to i8**
   store volatile i8* %6, i8** %addr, align 8, !dbg !763, !tbaa !764

   clang-format on
 */
static bool
visitIntStoreInst2(StoreInst *SI, Function &F, const DataLayout &DL,
                   SmallPtrSetImpl<StoreInst *> &StoresToErase,
                   SmallPtrSetImpl<Instruction *> &MaybeUnusedInsts) {

  auto *P2I = dyn_cast<PtrToIntInst>(SI->getValueOperand());
  if (!P2I) return false;

  IRBuilder<> IRB(SI);

  auto *Int8PtrTy = Type::getInt8PtrTy(SI->getContext());
  auto *Int8PtrPtrTy = Int8PtrTy->getPointerTo();

  auto *val = P2I->getPointerOperand();
  if (val->getType() != Int8PtrTy) val = IRB.CreateBitCast(val, Int8PtrTy);

  auto *addr = SI->getPointerOperand();
  if (addr->getType() != Int8PtrPtrTy)
    addr = IRB.CreateBitCast(addr, Int8PtrPtrTy);

  auto newSI = IRB.CreateStore(val, addr);
  newSI->setAlignment(SI->getAlign());
  newSI->setOrdering(SI->getOrdering());
  newSI->setSyncScopeID(SI->getSyncScopeID());
  newSI->setVolatile(SI->isVolatile());
  newSI->copyMetadata(*SI);

  DOG(llvm::errs() << "Replaced\n" << *SI << "\nby\n" << *newSI << "\n";);

  StoresToErase.insert(SI);
  MaybeUnusedInsts.insert(P2I);
  return true;
}

static bool visitStoreInst(StoreInst *SI, Function &F, const DataLayout &DL,
                           SmallPtrSetImpl<StoreInst *> &StoresToErase,
                           SmallPtrSetImpl<Instruction *> &MaybeUnusedInsts) {
  // -- clang-format off
  /*

    Rewrites the following use of ptrtoint

    %20 = ptrtoint %struct.Entry* %m to i32
    %22 = getelementptr inbounds %struct.Entry, %struct.Entry* %G, i32 0, i32 1
    %23 = bitcast %struct.Entry** %22 to i32*
    store i32 %20, i32* %23

    Into

    %22 = getelementptr inbounds %struct.Entry, %struct.Entry* %G, i32 0, i32 1
    store %struct.Entry* %m, %struct.Entry** %22

   */
  // -- clang-format on
  assert(SI);

  auto *P2I = dyn_cast<PtrToIntInst>(SI->getValueOperand());
  if (!P2I) return false;

  auto *BC = dyn_cast<BitCastInst>(SI->getPointerOperand());
  if (!BC) return false;

  auto *PtrVal = P2I->getPointerOperand();
  auto *PtrIntTy = DL.getIntPtrType(PtrVal->getType());

  if (PtrIntTy != P2I->getType()) return false;

  auto *BCTy = BC->getType();
  if (!BCTy->isPointerTy() || BCTy->getPointerElementType() != PtrIntTy)
    return false;

  auto *BCVal = BC->getOperand(0);
  auto *NewStoreDestTy = BCVal->getType();
  auto *NewStoreValTy = NewStoreDestTy->getPointerElementType();

  IRBuilder<> IRB(SI);
  if (!NewStoreValTy->isPointerTy()) {
    // -- if the store value type is not a pointer, but what is being stored is
    // i8*
    // -- then store through i8** pointer instead
    if (PtrVal->getType()->isPointerTy() &&
        PtrVal->getType()->getPointerElementType()->isIntegerTy(8)) {
      NewStoreValTy = IRB.getInt8PtrTy();
      BCVal = IRB.CreateBitCast(BCVal, NewStoreValTy->getPointerTo());
    } else {
      return false;
    }
  }

  DOG(llvm::errs() << "RP2I: cleaning up: " << *SI << "\n";);
  auto *NewBC = PtrVal->getType() != NewStoreValTy
                    ? IRB.CreateBitCast(PtrVal, NewStoreValTy)
                    : PtrVal;
  auto *res = IRB.CreateStore(NewBC, BCVal);

  res->setVolatile(SI->isVolatile());
  res->setAlignment(SI->getAlign());
  res->setOrdering(SI->getOrdering());
  res->setSyncScopeID(SI->getSyncScopeID());
  res->copyMetadata(*SI);

  // Collect instructions to erase. Deffer that not to invalidate iterators.
  StoresToErase.insert(SI);
  MaybeUnusedInsts.insert(P2I);
  MaybeUnusedInsts.insert(BC);

  return true;
}

/** Promotes integer expression to pointers to eliminate inttoptr instructions
 */
class PointerPromoter : public InstVisitor<PointerPromoter, Value *> {
  Type *m_ty;
  SmallPtrSetImpl<Instruction *> &m_toRemove;
  DenseMap<Value*, Value*> &m_toReplace;
  // -- to break cycles in PHINodes
  SmallPtrSet<Instruction *, 16> m_visited;

public:
  PointerPromoter(SmallPtrSetImpl<Instruction *> &toRemove,
                  DenseMap<Value *, Value *> &toReplace)
    : m_ty(nullptr), m_toRemove(toRemove), m_toReplace(toReplace) {}

  Value *visitInstruction(Instruction &I) { return nullptr; }

  Value *visitPtrToIntInst(Instruction &I) {
    // errs() << "Visiting: " << I << "\n";
    m_toRemove.insert(&I);

    auto *op = I.getOperand(0);
    if (op->getType() == m_ty) return op;
    IRBuilder<> IRB(&I);
    return IRB.CreateBitCast(op, m_ty);
  }

  Value *visitPHINode(PHINode &I) {
    // -- cycle, can only happen over phi-nodes
    // XXX This might prevent more than cycles ...
    if (m_visited.count(&I)) return nullptr;

    // errs() << "Visiting: " << I << "\n";
    m_visited.insert(&I);
    std::vector<Value *> vals(I.getNumIncomingValues());

    for (unsigned i = 0, e = I.getNumIncomingValues(); i < e; ++i) {
      auto *val = dyn_cast<Instruction>(I.getIncomingValue(i));
      if (!val) return nullptr;
      auto *ptr = this->visit(val);
      if (!ptr) return nullptr;
      vals[i] = ptr;
    }

    IRBuilder<> IRB(&I);
    auto *newPhi = IRB.CreatePHI(m_ty, I.getNumIncomingValues());

    for (unsigned i = 0, e = I.getNumIncomingValues(); i < e; ++i) {
      newPhi->addIncoming(vals[i], I.getIncomingBlock(i));
    }
    m_toRemove.insert(&I);
    return newPhi;
  }

  Value *visitAdd(BinaryOperator &I) {
    // errs() << "visitAdd: " << I << "\n";
    auto *op0 = dyn_cast<Instruction>(I.getOperand(0));
    if (!op0) return nullptr;

    auto *ptr = this->visit(op0);
    if (!ptr) return nullptr;

    m_toRemove.insert(&I);

    IRBuilder<> IRB(&I);

    ptr = IRB.CreateBitCast(ptr, IRB.getInt8PtrTy());
    auto *gep = IRB.CreateGEP(ptr->getType()->getScalarType()->getPointerElementType(),
                              ptr, I.getOperand(1));
    return IRB.CreateBitCast(gep, m_ty);
  }

  Value *visitSub(BinaryOperator &I) {
    // errs() << "visitSub: " << I << "\n";
    if (ConstantInt *CI = dyn_cast<ConstantInt>(I.getOperand(1))) {

      auto *op0 = dyn_cast<Instruction>(I.getOperand(0));
      if (!op0) return nullptr;
      
      auto *ptr = this->visit(op0);
      if (!ptr) return nullptr;
      
      m_toRemove.insert(&I);

      if (CI->isZero()) { return ptr; }
      
      IRBuilder<> IRB(&I);
      ptr = IRB.CreateBitCast(ptr, IRB.getInt8PtrTy());
      const APInt &opV = CI->getValue(); 
      auto *gep =
        IRB.CreateGEP(ptr->getType()->getScalarType()->getPointerElementType(),
                      ptr, ConstantInt::get(CI->getType(), opV * -1));
      return IRB.CreateBitCast(gep, m_ty);
    } else {
      // TODO: a non-constant operand
    return nullptr;
  }
  }

  Value *visitLoad(LoadInst &I) {
    assert(m_ty);
    // errs() << "visitLoad: " << I << "\n";
    auto *ptr = I.getPointerOperand();
    IRBuilder<> IRB(&I);
    ptr = IRB.CreateBitCast(ptr, m_ty->getPointerTo());
    auto *res = IRB.CreateLoad(ptr->getType()->getPointerElementType(), ptr);
    res->setVolatile(I.isVolatile());
    res->setAlignment(I.getAlign());
    res->setOrdering(I.getOrdering());
    res->setSyncScopeID(I.getSyncScopeID());
    res->copyMetadata(I);
    // -- clear range metadata since it is no longer valid
    res->setMetadata(LLVMContext::MD_range, nullptr);

    Value *p2i = nullptr;
    for (auto *u : I.users()) {
      if (auto *SI = dyn_cast<StoreInst>(u)) {
        if (SI->getValueOperand() != &I) { continue; }
        IRB.SetInsertPoint(SI);
        auto *ptr = SI->getPointerOperand();
        ptr = IRB.CreateBitCast(ptr, m_ty->getPointerTo());
        auto newStore = IRB.CreateStore(res, ptr);
        newStore->setVolatile(SI->isVolatile());
        newStore->setAlignment(SI->getAlign());
        newStore->setOrdering(SI->getOrdering());
        newStore->setSyncScopeID(SI->getSyncScopeID());
        newStore->copyMetadata(*SI);

        m_toRemove.insert(SI);
      } else if (isa<IntToPtrInst>(u)) {
        /* skip  */;
      } else  {
        // -- if there is an interesting user, schedule it to be replaced
        if (!p2i) {
          IRB.SetInsertPoint(&I);
          p2i = IRB.CreatePtrToInt(res, I.getType());
        }
        m_toReplace.insert({&I, p2i});
      }
    }

    return res;
  }

  Value *visitIntToPtrInst(IntToPtrInst &I) {
    // errs() << "Visiting: " << I << "\n";

    assert(!m_ty);
    m_ty = I.getType();

    auto *op = dyn_cast<Instruction>(I.getOperand(0));
    Value *ret = nullptr;
    if (op) ret = this->visit(*op);

    if (ret) {
      I.replaceAllUsesWith(ret);
      m_toRemove.insert(&I);
    }

    m_ty = nullptr;
    return ret;
  }
};

static bool visitIntToPtrInst(IntToPtrInst *I2P, Function &F,
                              const DataLayout &DL, DominatorTree &DT,
                  SmallDenseMap<PHINode *, PHINode *> &NewPhis,
                  SmallPtrSetImpl<Instruction *> &MaybeUnusedInsts,
                  DenseMap<Value*, Value*> &RenameMap) {
  assert(I2P);

  auto *IntVal = I2P->getOperand(0);

  auto *PN = dyn_cast<PHINode>(IntVal);
  if (!PN) {
    PointerPromoter pp(MaybeUnusedInsts, RenameMap);
    return pp.visit(I2P);
    return false;
  }

  DOG(errs() << F.getName() << ":\n"; I2P->print(errs());
      PN->print(errs() << "\n"); errs() << "\n");

  if (NewPhis.count(PN) > 0) {
    IRBuilder<> IRB(I2P);
    auto *NewPhi = NewPhis[PN];
    I2P->replaceAllUsesWith(IRB.CreateBitCast(NewPhi, I2P->getType()));
    MaybeUnusedInsts.insert(I2P);

    DOG(errs() << "\n!!!! Reused new PHI ~~~~~ \n");
    return true;
  }

  if (!llvm::all_of(PN->incoming_values(),
                    [](Value *IVal) { return isa<PtrToIntInst>(IVal); })) {
    DOG(errs() << "Not all incoming values are PtrToInstInsts, skipping PHI\n");
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
      // assert(DT.dominates(P2I, I2P));

      auto *Ptr = P2I->getPointerOperand();
      assert(Ptr);

      IRB.SetInsertPoint(P2I);
      NewPN->addIncoming(IRB.CreateBitCast(Ptr, I2P->getType()), IBB);
      DOG(NewPN->print(errs(), 1));

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
    //      I2P->getType()->getPointerTo(0), Loaded->getName() + ".rpti");
    //      auto *NewLI = IRB.CreateLoad(CastedLoaded, LI->getName() +
    //      ".rpti");
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
  if (F.isDeclaration()) return false;

  // Skip special functions
  if (F.getName().startswith("seahorn.") || F.getName().startswith("shadow.") ||
      F.getName().startswith("verifier."))
    return false;

  DOG(errs() << "\n~~~~~~~ Begin of RP2I on " << F.getName() << " ~~~~~ \n");

  bool Changed = false;
  auto &DL = F.getParent()->getDataLayout();
  // Would be better to get the DominatorTreeWrapperPass, but the pass manager
  // crashed when it's required.
  auto &DT = getAnalysis<DominatorTreeWrapperPass>().getDomTree();
  SmallPtrSet<StoreInst *, 8> StoresToErase;
  SmallPtrSet<Instruction *, 16> MaybeUnusedInsts;
  DenseMap<Value*, Value*> RenameMap;
  SmallDenseMap<PHINode *, PHINode *> NewPhis;

  for (auto &BB : F) {
    for (auto &I : BB) {
      if (auto *SI = dyn_cast<StoreInst>(&I)) {
        bool flag = false;
        flag |= visitIntStoreInst2(SI, F, DL, StoresToErase, MaybeUnusedInsts);
        flag |= visitStoreInst(SI, F, DL, StoresToErase, MaybeUnusedInsts);
        flag |= visitIntStoreInst(SI, F, DL, StoresToErase, MaybeUnusedInsts);
        Changed |= flag;
        continue;
      }

      if (auto *I2P = dyn_cast<IntToPtrInst>(&I)) {
        Changed |= visitIntToPtrInst(I2P, F, DL, DT, NewPhis, MaybeUnusedInsts,
                                     RenameMap);
        continue;
      }
    }
  }

  for (auto *SI : StoresToErase) {
    MaybeUnusedInsts.erase(SI);
    SI->eraseFromParent();
  }

  SmallVector<Instruction *, 16> TriviallyDeadInstructions(
      llvm::make_filter_range(MaybeUnusedInsts, [](Instruction *I) {
        return isInstructionTriviallyDead(I);
      }));
  
  // TODO: RecursivelyDeleteTriviallyDeadInstructions takes a vector of
  // WeakTrackingVH since LLVM 11, which implies that `MaybeUnusedInsts` should
  // be a vector of WeakTrackingVH as well. See comment:
  // https://github.com/seahorn/sea-dsa/pull/118#discussion_r587061761.
  // However, the instructions currently stored in `MaybeUnusedInsts` are not
  // removed before this call. So it should be safe to keep its original type
  // and map it to WeakTrackingVH as the following code does.
  SmallVector<WeakTrackingVH, 16> TriviallyDeadHandles;
  for (auto i : TriviallyDeadInstructions)  
      TriviallyDeadHandles.emplace_back(WeakTrackingVH(i));
  
  RecursivelyDeleteTriviallyDeadInstructions(TriviallyDeadHandles);
  for (auto kv : RenameMap) {
    kv.first->replaceAllUsesWith(kv.second);
  }

  DOG(llvm::errs() << "\n~~~~~~~ End of RP2I on " << F.getName() << " ~~~~~ \n";
      llvm::errs().flush());

  if (Changed) { assert(!llvm::verifyFunction(F, &llvm::errs())); }

  // llvm::errs() << F << "\n";
  return Changed;
}

char seadsa::RemovePtrToInt::ID = 0;

void RemovePtrToInt::getAnalysisUsage(llvm::AnalysisUsage &AU) const {
  AU.setPreservesCFG();
  AU.addRequired<llvm::DominatorTreeWrapperPass>();
  AU.addRequired<llvm::TargetLibraryInfoWrapperPass>();
}

Pass *createRemovePtrToIntPass() { return new RemovePtrToInt(); }
} // namespace seadsa

using namespace seadsa;
INITIALIZE_PASS_BEGIN(RemovePtrToInt, "sea-remove-ptrtoint",
                      "Remove ptrtoint instructions", false, false)
INITIALIZE_PASS_DEPENDENCY(DominatorTreeWrapperPass)
INITIALIZE_PASS_DEPENDENCY(TargetLibraryInfoWrapperPass)
INITIALIZE_PASS_END(RemovePtrToInt, "sea-remove-ptrtoint",
                    "Remove ptrtoint instructions", false, false)
