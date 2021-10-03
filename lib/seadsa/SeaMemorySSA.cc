#include "seadsa/SeaMemorySSA.hh"
#include "seadsa/support/Debug.h"

#include "llvm/IR/AssemblyAnnotationWriter.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/FormattedStream.h"

using namespace seadsa;
using namespace llvm;

namespace seadsa {
SeaMemorySSA::SeaMemorySSA(Function &Func, GlobalAnalysis &DSA)
    : F(Func), DSA(DSA), LiveOnEntryDefs(std::make_unique<AccessList>()),
      LiveOnExitUses(std::make_unique<AccessList>()), NextID(0) {}

SeaMemorySSA::~SeaMemorySSA() {
  // Drop all our references
  for (const auto &Pair : PerBlockAccesses)
    for (SeaMemoryAccess &MA : *Pair.second)
      MA.dropAllReferences();

  for (SeaMemoryAccess &MA : *LiveOnEntryDefs)
    MA.dropAllReferences();

  for (SeaMemoryAccess &MA : *LiveOnExitUses)
    MA.dropAllReferences();
}

SeaMemorySSA::AccessList *
SeaMemorySSA::getOrCreateAccessList(const BasicBlock *BB) {
  auto Res = PerBlockAccesses.insert(std::make_pair(BB, nullptr));

  if (Res.second) Res.first->second = std::make_unique<AccessList>();
  return Res.first->second.get();
}

class SeaMemorySSAAnnotatedWriter : public llvm::AssemblyAnnotationWriter {
  friend class SeaMemorySSA;

  const SeaMemorySSA *SMSSA;

public:
  SeaMemorySSAAnnotatedWriter(const SeaMemorySSA *M) : SMSSA(M) {}

  void emitFunctionAnnot(const Function *, formatted_raw_ostream &) override {}

  void emitBasicBlockStartAnnot(const BasicBlock *BB,
                                llvm::formatted_raw_ostream &OS) override {

    if (BB == &BB->getParent()->getEntryBlock()) {
      for (auto &MA : SMSSA->liveOnEntry())
        OS << "; " << MA << "\n";
    } else {
      for (auto &MA : SMSSA->getMemoryAccesses(BB)) {
        OS << "; " << MA << "\n";
      }
    }
  }

  const Instruction *getMemoryInst(const SeaMemoryAccess &MA) {
    if (auto *MUD = dyn_cast<SeaMemoryUseOrDef>(&MA)) {
      return MUD->getMemoryInst();
    }
    return nullptr;
  }

  void emitInstructionAnnot(const Instruction *I,
                            llvm::formatted_raw_ostream &OS) override {
    // const BasicBlock *BB = I->getParent();
    if (auto *RI = dyn_cast<ReturnInst>(I)) {
      for (auto &MA : SMSSA->liveOnExit())
        OS << "; " << MA << "\n";
    } else {
      for (auto &MA : SMSSA->getMemoryAccesses(I))
        OS << "; " << MA << " \n";
    }

    // else if (auto const *FirstMA = SMSSA->getMemoryAccess(I)) {
    //   // iterate over all MUD accesses corresponding to I
    //   for (auto &MA : llvm::make_range(FirstMA->getIterator(),
    //                                    SMSSA->getBlockAccesses(BB)->end())) {
    //     if (getMemoryInst(MA) != I) break;
    //     OS << "; " << MA << "\n";
    //   }
  }
};

void SeaMemorySSA::print(raw_ostream &OS) const {
  SeaMemorySSAAnnotatedWriter Writer(this);
  F.print(OS, &Writer);
}

#if !defined(NDEBUG) || defined(LLVM_ENABLE_DUMP)
LLVM_DUMP_METHOD void SeaMemorySSA::dump() const { print(dbgs()); }
#endif

const static char LiveOnEntryStr[] = "liveOnEntry";
void SeaMemoryAccess::print(raw_ostream &OS) const {
  switch (getValueID()) {
  case MemoryPhiVal:
    return static_cast<const SeaMemoryPhi *>(this)->print(OS);
  case MemoryDefVal:
    return static_cast<const SeaMemoryDef *>(this)->print(OS);
  case MemoryUseVal:
    return static_cast<const SeaMemoryUse *>(this)->print(OS);
  }
  llvm_unreachable("invalid value id");
}

void SeaMemoryDef::print(raw_ostream &OS) const {
  auto *UO = getDefiningAccess();

  auto printID = [&OS](SeaMemoryAccess *A) {
    if (A && A->getID())
      OS << A->getID();
    else
      OS << LiveOnEntryStr;
  };

  OS << getID() << " = MemoryDef(";
  printID(UO);
  OS << ")";

  if (isOptimized()) {
    OS << "->";
    printID(getOptimized());

    if (Optional<AliasResult> AR = getOptimizedAccessType()) OS << " " << *AR;
  }

  if (m_Cell.getNode()) {
    OS << " <" << m_Cell.getOffset() << " @ N(" << m_Cell.getNode()->getId()
       << ")>";
  }
}

void SeaMemoryPhi::print(raw_ostream &OS) const {
  bool First = true;
  OS << getID() << " = MemoryPhi(";
  for (const auto &Op : operands()) {
    BasicBlock *BB = getIncomingBlock(Op);
    auto *MA = cast<SeaMemoryAccess>(Op);
    if (!First)
      OS << ',';
    else
      First = false;

    OS << '{';
    if (BB->hasName())
      OS << BB->getName();
    else
      BB->printAsOperand(OS, false);
    OS << ',';
    if (unsigned ID = MA->getID())
      OS << ID;
    else
      OS << LiveOnEntryStr;
    OS << '}';
  }
  OS << ')';
}

void SeaMemoryUse::print(raw_ostream &OS) const {
  auto *UO = getDefiningAccess();
  OS << "MemoryUse(";
  if (UO && UO->getID())
    OS << UO->getID();
  else
    OS << LiveOnEntryStr;
  OS << ')';

  if (Optional<AliasResult> AR = getOptimizedAccessType()) OS << " " << *AR;
}

void SeaMemoryAccess::dump() const {
// Cannot completely remove virtual function even in release mode.
#if !defined(NDEBUG) || defined(LLVM_ENABLE_DUMP)
  print(dbgs());
  dbgs() << "\n";
#endif
}

void SeaMemoryPhi::deleteMe(DerivedUser *Self) {
  delete static_cast<MemoryPhi *>(Self);
}

void SeaMemoryDef::deleteMe(DerivedUser *Self) {
  delete static_cast<MemoryDef *>(Self);
}

void SeaMemoryUse::deleteMe(DerivedUser *Self) {
  delete static_cast<MemoryUse *>(Self);
}

static Instruction *findFirstNonShadowInst(Instruction &I) {
  BasicBlock *BB = I.getParent();
  for (auto &II : llvm::make_range(++BasicBlock::iterator(I), BB->end()))
    if (!isShadowMemInst(II)) return &II;
  return nullptr;
}

namespace {
/// Context used by SeaMemorySSA::buildForFunction
struct ResolveContext {
  SeaMemoryPhi *m_memPhi;
  const Value *m_value;
  unsigned m_operand;
  ResolveContext(SeaMemoryPhi *memPhi, const Value *value, unsigned operand)
      : m_memPhi(memPhi), m_value(value), m_operand(operand) {}
};
} // namespace

void SeaMemorySSA::buildForFunction(ShadowMem &shadow) {
  // from Value to defining Memory Access (SeaMemoryDef or SeaMemoryPhi)
  DenseMap<Value *, SeaMemoryAccess *> defMap;
  SmallVector<ResolveContext, 64> ToResolve;

  for (auto &I : llvm::instructions(F)) {
    if (!isShadowMemInst(I)) continue;

    BasicBlock *BB = I.getParent();
    LLVMContext &C = BB->getContext();
    AccessList *acl = getOrCreateAccessList(BB);

    SeaMemoryAccess *DMA = nullptr;
    Instruction *MI = nullptr;
    std::pair<llvm::Value *, llvm::Value *> DUP; // Def-Use-Pair

    SeaMemoryDef *newDef = nullptr;
    SeaMemoryUse *newUse = nullptr;
    if (auto *CI = dyn_cast<CallInst>(&I)) {
      auto MemOp = shadow.getShadowMemOp(*CI);
      assert(MemOp != ShadowMemInstOp::UNKNOWN);
      switch (MemOp) {
      default:
        llvm_unreachable("Unknown Op");
      case ShadowMemInstOp::FUN_IN:
      case ShadowMemInstOp::FUN_OUT:
        // these uses don't belong to the basic block they appear in
        DUP = shadow.getShadowMemVars(*CI);
        assert(DUP.second);
        DMA = defMap.lookup(DUP.second);
        assert(DMA);
        newUse = new SeaMemoryUse(C, DMA, nullptr, nullptr);
        LiveOnExitUses->push_back(newUse);
        LiveOnExitUses->back().setShadowMemOp(MemOp);
        ShadowValueToMemoryAccess.insert({CI, newUse});
        break;
      case ShadowMemInstOp::LOAD:
      case ShadowMemInstOp::TRSFR_LOAD:
      case ShadowMemInstOp::ARG_REF:
        MI = findFirstNonShadowInst(I);
        DUP = shadow.getShadowMemVars(*CI);
        assert(DUP.second);
        DMA = defMap.lookup(DUP.second);
        assert(DMA);
        newUse = new SeaMemoryUse(C, DMA, MI, BB);
        newUse->setShadowMemOp(MemOp);
        acl->push_back(newUse);
        ValueToMemoryAccess.insert({MI, newUse});
        ShadowValueToMemoryAccess.insert({CI, newUse});
        break;
      case ShadowMemInstOp::GLOBAL_INIT:
      case ShadowMemInstOp::INIT:
      case ShadowMemInstOp::ARG_INIT:
        DUP = shadow.getShadowMemVars(*CI);
        assert(DUP.first);
        DMA = DUP.second ? defMap.lookup(DUP.second) : nullptr;
        newDef = new SeaMemoryDef(C, DMA, nullptr, BB, ++NextID);
        newDef->setShadowMemOp(MemOp);
        newDef->setDsaCell(shadow.getShadowMemCell(*CI));
        LiveOnEntryDefs->push_back(newDef);
        defMap.insert({DUP.first, newDef});
        ShadowValueToMemoryAccess.insert({CI, newDef});
        break;
      case ShadowMemInstOp::STORE:
      case ShadowMemInstOp::ARG_MOD:
      case ShadowMemInstOp::ARG_NEW:
        MI = findFirstNonShadowInst(I);
        DUP = shadow.getShadowMemVars(*CI);
        assert(DUP.first);
        // DMA is null for def that is live on entry
        DMA = DUP.second ? defMap.lookup(DUP.second) : nullptr;
        assert(DMA);
        newDef = new SeaMemoryDef(C, DMA, MI, BB, ++NextID);
        newDef->setShadowMemOp(MemOp);
        newDef->setDsaCell(shadow.getShadowMemCell(*CI));
        acl->push_back(newDef);
        defMap.insert({DUP.first, newDef});
        ValueToMemoryAccess.insert({MI, newDef});
        ShadowValueToMemoryAccess.insert({CI, newDef});
        break;
      }
    } else if (auto *PHI = dyn_cast<PHINode>(&I)) {
      SeaMemoryPhi *newPhi =
          new SeaMemoryPhi(C, BB, ++NextID, PHI->getNumIncomingValues());
      for (BasicBlock *incomingBB : PHI->blocks()) {
        Value *incomingValue = PHI->getIncomingValueForBlock(incomingBB);
        auto *incomingMA = defMap.lookup(incomingValue);
        if (incomingMA) {
          newPhi->addIncoming(incomingMA, incomingBB);
        } else {
          // -- create temporary incoming value of nullptr
          newPhi->addIncoming(nullptr, incomingBB);
          /// -- resolve later based on information from the current context
          ToResolve.emplace_back(newPhi, incomingValue,
                                 newPhi->getNumOperands() - 1);
        }
      }
      acl->push_back(newPhi);
      defMap.insert({PHI, newPhi});
      // inserts only the first one PhiNode
      ValueToMemoryAccess.insert({BB, newPhi});
      ShadowValueToMemoryAccess.insert({PHI, newPhi});
    } else
      llvm_unreachable("Unexpected shadow.mem instructions");
  }

  // -- resolve all incoming values that could not be filled in the first pass
  for (auto &Ctx : ToResolve) {
    auto *inMA = defMap.lookup(Ctx.m_value);
    assert(inMA);
    Ctx.m_memPhi->setIncomingValue(Ctx.m_operand, inMA);
  }
  LOG("memssa", errs() << "ORIGINAL FUNCTION:\n"
                       << F << "\n";
      this->print(errs()); errs() << "\n";);
}
} // namespace seadsa
