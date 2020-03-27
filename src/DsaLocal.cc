#include "sea_dsa/Local.hh"

#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/Analysis/CFG.h"
#include "llvm/Analysis/MemoryBuiltins.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/GetElementPtrTypeIterator.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/PatternMatch.h"

#include "sea_dsa/AllocWrapInfo.hh"
#include "sea_dsa/Graph.hh"
#include "sea_dsa/TypeUtils.hh"
#include "sea_dsa/support/Debug.h"

#include "boost/range/algorithm/reverse.hpp"

using namespace llvm;

namespace llvm {

static llvm::cl::opt<bool> TrustArgumentTypes(
    "sea-dsa-trust-args",
    llvm::cl::desc("Trust function argument types in SeaDsa Local analysis"),
    llvm::cl::init(true));

// borrowed from SeaHorn
class __BlockedEdges {
  llvm::SmallPtrSet<const BasicBlock *, 8> m_set;
  llvm::SmallVectorImpl<std::pair<const BasicBlock *, const BasicBlock *>>
      &m_edges;

public:
  __BlockedEdges(
      llvm::SmallVectorImpl<std::pair<const BasicBlock *, const BasicBlock *>>
          &edges)
      : m_edges(edges) {
    std::sort(m_edges.begin(), m_edges.end());
  }

  bool insert(const BasicBlock *bb) { return m_set.insert(bb).second; }

  bool isBlockedEdge(const BasicBlock *src, const BasicBlock *dst) {
    return std::binary_search(m_edges.begin(), m_edges.end(),
                              std::make_pair(src, dst));
  }
};

template <> class po_iterator_storage<__BlockedEdges, true> {
  __BlockedEdges &Visited;

public:
  po_iterator_storage(__BlockedEdges &VSet) : Visited(VSet) {}
  po_iterator_storage(const po_iterator_storage &S) : Visited(S.Visited) {}

  bool insertEdge(Optional<const BasicBlock *> src, const BasicBlock *dst) {
    return Visited.insert(dst);
  }
  void finishPostorder(const BasicBlock *bb) {}
};

} // namespace llvm

namespace {

// borrowed from seahorn
void revTopoSort(const llvm::Function &F,
                 std::vector<const BasicBlock *> &out) {
  if (F.isDeclaration() || F.empty())
    return;

  llvm::SmallVector<std::pair<const BasicBlock *, const BasicBlock *>, 8>
      backEdges;
  FindFunctionBackedges(F, backEdges);

  const llvm::Function *f = &F;
  __BlockedEdges ble(backEdges);
  std::copy(po_ext_begin(f, ble), po_ext_end(f, ble), std::back_inserter(out));
}

std::pair<int64_t, uint64_t>
computeGepOffset(Type *ptrTy, ArrayRef<Value *> Indicies, const DataLayout &dl);

template <typename T> T gcd(T a, T b) {
  T c;
  while (b) {
    c = a % b;
    a = b;
    b = c;
  }
  return a;
}

class BlockBuilderBase {
protected:
  Function &m_func;
  sea_dsa::Graph &m_graph;
  const DataLayout &m_dl;
  const TargetLibraryInfo &m_tli;
  const sea_dsa::AllocWrapInfo &m_allocInfo;

  sea_dsa::Cell valueCell(const Value &v);
  void visitGep(const Value &gep, const Value &base,
                ArrayRef<Value *> indicies);
  void visitCastIntToPtr(const Value &dest);

  bool isFixedOffset(const IntToPtrInst &inst, Value *&base, uint64_t &offset);
  bool isSkip(Value &V) {
    if (!V.getType()->isPointerTy())
      return true;
    // XXX skip if only uses are external functions
    return false;
  }

  static bool containsPointer(const Value &V) {
    SmallVector<const Type *, 16> workList;
    SmallPtrSet<const Type *, 16> seen;
    workList.push_back(V.getType());

    while (!workList.empty()) {
      const Type *Ty = workList.back();
      workList.pop_back();
      if (!seen.insert(Ty).second)
        continue;

      if (Ty->isPointerTy()) {
        return true;
      }

      if (const StructType *ST = dyn_cast<StructType>(Ty)) {
        for (unsigned i = 0, sz = ST->getNumElements(); i < sz; ++i) {
          workList.push_back(ST->getElementType(i));
        }
      } else if (const SequentialType *ST = dyn_cast<SequentialType>(Ty)) {
        // ArrayType and VectorType are subclasses of SequentialType
        workList.push_back(ST->getElementType());
      }
    }
    return false;
  }

  static bool isNullConstant(const Value &v) {
    const Value *V = v.stripPointerCasts();

    if (isa<Constant>(V) && cast<Constant>(V)->isNullValue())
      return true;

    // XXX: some linux device drivers contain instructions gep null, ....
    if (const GetElementPtrInst *Gep = dyn_cast<const GetElementPtrInst>(V)) {
      const Value &base = *Gep->getPointerOperand();
      if (const Constant *c = dyn_cast<Constant>(base.stripPointerCasts()))
        return c->isNullValue();
    }

    return false;
  }

public:
  BlockBuilderBase(Function &func, sea_dsa::Graph &graph, const DataLayout &dl,
                   const TargetLibraryInfo &tli,
                   const sea_dsa::AllocWrapInfo &allocInfo)
      : m_func(func), m_graph(graph), m_dl(dl), m_tli(tli),
        m_allocInfo(allocInfo) {}
};

class GlobalBuilder : public BlockBuilderBase {

  /// from: llvm/lib/ExecutionEngine/ExecutionEngine.cpp
  void init(const Constant *Init, sea_dsa::Cell &c, unsigned offset) {
    if (isa<UndefValue>(Init)) {
      return;
    }

    if (const ConstantVector *CP = dyn_cast<ConstantVector>(Init)) {
      unsigned ElementSize =
          m_dl.getTypeAllocSize(CP->getType()->getElementType());
      for (unsigned i = 0, e = CP->getNumOperands(); i != e; ++i) {
        unsigned noffset = offset + i * ElementSize;
        sea_dsa::Cell nc = sea_dsa::Cell(c.getNode(), noffset);
        init(CP->getOperand(i), nc, noffset);
      }
      return;
    }

    if (isa<ConstantAggregateZero>(Init)) {
      return;
    }

    if (const ConstantArray *CPA = dyn_cast<ConstantArray>(Init)) {
      unsigned ElementSize =
          m_dl.getTypeAllocSize(CPA->getType()->getElementType());
      for (unsigned i = 0, e = CPA->getNumOperands(); i != e; ++i) {
        unsigned noffset = offset + i * ElementSize;
        sea_dsa::Cell nc = sea_dsa::Cell(c.getNode(), noffset);
        init(CPA->getOperand(i), nc, noffset);
      }
      return;
    }

    if (const ConstantStruct *CPS = dyn_cast<ConstantStruct>(Init)) {
      const StructLayout *SL =
          m_dl.getStructLayout(cast<StructType>(CPS->getType()));
      for (unsigned i = 0, e = CPS->getNumOperands(); i != e; ++i) {
        unsigned noffset = offset + SL->getElementOffset(i);
        sea_dsa::Cell nc = sea_dsa::Cell(c.getNode(), noffset);
        init(CPS->getOperand(i), nc, noffset);
      }
      return;
    }

    if (isa<ConstantDataSequential>(Init)) {
      return;
    }

    if (Init->getType()->isPointerTy() && !isa<ConstantPointerNull>(Init)) {
      if (cast<PointerType>(Init->getType())
              ->getElementType()
              ->isFunctionTy()) {
        sea_dsa::Node &n = m_graph.mkNode();
        sea_dsa::Cell nc(n, 0);
        sea_dsa::DsaAllocSite *site = m_graph.mkAllocSite(*Init);
        assert(site);
        n.addAllocSite(*site);

        // connect c with nc
        c.growSize(0, Init->getType());
        c.addAccessedType(0, Init->getType());
        c.addLink(sea_dsa::Field(0, sea_dsa::FieldType(Init->getType())), nc);
        return;
      }

      if (m_graph.hasCell(*Init)) {
        // @g1 =  ...*
        // @g2 =  ...** @g1
        sea_dsa::Cell &nc = m_graph.mkCell(*Init, sea_dsa::Cell());
        // connect c with nc
        c.growSize(0, Init->getType());
        c.addAccessedType(0, Init->getType());
        c.addLink(sea_dsa::Field(0, sea_dsa::FieldType(Init->getType())), nc);
        return;
      }
    }
  }

public:
  // XXX: it should take a module but we want to reuse
  // BlockBuilderBase so we need to pass a function. We assume that
  // the passed function is main.
  GlobalBuilder(Function &func, sea_dsa::Graph &graph, const DataLayout &dl,
                const TargetLibraryInfo &tli,
                const sea_dsa::AllocWrapInfo &allocInfo)
      : BlockBuilderBase(func, graph, dl, tli, allocInfo) {}

  void initGlobalVariables() {
    if (!m_func.getName().equals("main")) {
      return;
    }

    Module &M = *(m_func.getParent());
    for (auto &gv : M.globals()) {
      if (gv.getName().equals("llvm.used"))
        continue;

      if (gv.hasInitializer()) {
        sea_dsa::Cell c = valueCell(gv);
        init(gv.getInitializer(), c, 0);
      }
    }
  }
};

class InterBlockBuilder : public InstVisitor<InterBlockBuilder>,
                          BlockBuilderBase {
  friend class InstVisitor<InterBlockBuilder>;

  void visitPHINode(PHINode &PHI);

public:
  InterBlockBuilder(Function &func, sea_dsa::Graph &graph, const DataLayout &dl,
                    const TargetLibraryInfo &tli,
                    const sea_dsa::AllocWrapInfo &allocInfo)
      : BlockBuilderBase(func, graph, dl, tli, allocInfo) {}
};

void InterBlockBuilder::visitPHINode(PHINode &PHI) {
  if (!PHI.getType()->isPointerTy())
    return;

  assert(m_graph.hasCell(PHI));
  sea_dsa::Cell &phi = m_graph.mkCell(PHI, sea_dsa::Cell());
  for (unsigned i = 0, e = PHI.getNumIncomingValues(); i < e; ++i) {
    Value &v = *PHI.getIncomingValue(i);
    // -- skip null
    if (isa<Constant>(&v) && cast<Constant>(&v)->isNullValue())
      continue;

    // -- skip undef
    if (isa<Constant>(&v) && isa<UndefValue>(&v))
      continue;

    sea_dsa::Cell c = valueCell(v);
    if (c.isNull()) {
      // -- skip null: special case from ldv benchmarks
      if (const GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(&v)) {
        if (BlockBuilderBase::isNullConstant(
                *(GEP->getPointerOperand()->stripPointerCasts())))
          continue;
      }
    }
    assert(!c.isNull());
    phi.unify(c);
  }
  assert(!phi.isNull());
}

class IntraBlockBuilder : public InstVisitor<IntraBlockBuilder>,
                          BlockBuilderBase {
  friend class InstVisitor<IntraBlockBuilder>;

  // -- whether or not create a cell for an indirect callee
  bool m_track_callsites;

  void visitAllocaInst(AllocaInst &AI);
  void visitSelectInst(SelectInst &SI);
  void visitLoadInst(LoadInst &LI);
  void visitStoreInst(StoreInst &SI);
  void visitAtomicCmpXchgInst(AtomicCmpXchgInst &I);
  void visitAtomicRMWInst(AtomicRMWInst &I);
  void visitReturnInst(ReturnInst &RI);
  // void visitVAArgInst(VAArgInst   &I);
  void visitIntToPtrInst(IntToPtrInst &I);
  void visitPtrToIntInst(PtrToIntInst &I);
  void visitBitCastInst(BitCastInst &I);
  void visitCmpInst(CmpInst &I) { /* do nothing */
  }
  void visitInsertValueInst(InsertValueInst &I);
  void visitExtractValueInst(ExtractValueInst &I);
  void visitShuffleVectorInst(ShuffleVectorInst &I);

  void visitGetElementPtrInst(GetElementPtrInst &I);
  void visitInstruction(Instruction &I);

  void visitMemSetInst(MemSetInst &I);
  void visitMemTransferInst(MemTransferInst &I);

  void visitCallSite(CallSite CS);
  // void visitVAStart(CallSite CS);

  static bool isSeaDsaAliasFn(const Function *F) {
    return (F->getName().equals("sea_dsa_alias"));
  }

  static bool isSeaDsaCollapseFn(const Function *F) {
    return (F->getName().equals("sea_dsa_collapse"));
  }

  static bool isSeaDsaMkSequenceFn(const Function *F) {
    return (F->getName().equals("sea_dsa_mk_seq"));
  }

public:
  IntraBlockBuilder(Function &func, sea_dsa::Graph &graph, const DataLayout &dl,
                    const TargetLibraryInfo &tli,
                    const sea_dsa::AllocWrapInfo &allocInfo,
                    bool track_callsites)
      : BlockBuilderBase(func, graph, dl, tli, allocInfo),
        m_track_callsites(track_callsites) {}
};

sea_dsa::Cell BlockBuilderBase::valueCell(const Value &v) {
  using namespace sea_dsa;

  if (isNullConstant(v)) {
    LOG("dsa", errs() << "WARNING: constant not handled: " << v << "\n";);
    return Cell();
  }

  if (m_graph.hasCell(v)) {
    Cell &c = m_graph.mkCell(v, Cell());
    assert(!c.isNull());
    return c;
  }

  if (isa<UndefValue>(&v))
    return Cell();
  if (isa<GlobalAlias>(&v))
    return valueCell(*cast<GlobalAlias>(&v)->getAliasee());

  if (isa<ConstantStruct>(&v) || isa<ConstantArray>(&v) ||
      isa<ConstantDataSequential>(&v) || isa<ConstantDataArray>(&v) ||
      isa<ConstantVector>(&v) || isa<ConstantDataVector>(&v)) {
    // XXX Handle properly once we have real examples with this failure
    LOG("dsa", errs() << "WARNING: unsound handling of a constant: " << v << "\n";);
    // llvm_unreachable("Constant not handled!");
    return m_graph.mkCell(v, Cell(m_graph.mkNode(), 0));
  }

  // -- special case for aggregate types. Cell creation is handled elsewhere
  if (v.getType()->isAggregateType())
    return Cell();

  if (const ConstantExpr *ce = dyn_cast<const ConstantExpr>(&v)) {
    if (ce->isCast() && ce->getOperand(0)->getType()->isPointerTy())
      return valueCell(*ce->getOperand(0));
    else if (ce->getOpcode() == Instruction::GetElementPtr) {
      Value &base = *(ce->getOperand(0));
      SmallVector<Value *, 8> indicies(ce->op_begin() + 1, ce->op_end());
      visitGep(v, base, indicies);
      assert(m_graph.hasCell(v));
      return m_graph.mkCell(v, Cell());
    } else if (ce->getOpcode() == Instruction::IntToPtr) {
      visitCastIntToPtr(*ce);
      assert(m_graph.hasCell(*ce));
      return m_graph.mkCell(*ce, Cell());
    }
  }

  const auto &vType = *v.getType();
  assert(vType.isPointerTy() || vType.isAggregateType() || vType.isVectorTy());
  (void)vType;

  errs() << "Unexpected expression at valueCell: " << v << "\n";
  assert(false && "Expression not handled");
  return Cell();
}

void IntraBlockBuilder::visitInstruction(Instruction &I) {
  if (isSkip(I))
    return;

  m_graph.mkCell(I, sea_dsa::Cell(m_graph.mkNode(), 0));
}

void IntraBlockBuilder::visitAllocaInst(AllocaInst &AI) {
  using namespace sea_dsa;
  assert(!m_graph.hasCell(AI));
  Node &n = m_graph.mkNode();
  // -- record allocation site
  sea_dsa::DsaAllocSite *site = m_graph.mkAllocSite(AI);
  assert(site);
  n.addAllocSite(*site);
  // -- mark node as a stack node
  n.setAlloca();

  m_graph.mkCell(AI, Cell(n, 0));
}

void IntraBlockBuilder::visitSelectInst(SelectInst &SI) {
  using namespace sea_dsa;
  if (isSkip(SI))
    return;

  assert(!m_graph.hasCell(SI));

  Cell thenC = valueCell(*SI.getOperand(1));
  Cell elseC = valueCell(*SI.getOperand(2));
  thenC.unify(elseC);

  // -- create result cell
  m_graph.mkCell(SI, Cell(thenC, 0));
}

void IntraBlockBuilder::visitLoadInst(LoadInst &LI) {
  using namespace sea_dsa;

  // -- skip read from NULL
  if (BlockBuilderBase::isNullConstant(
          *LI.getPointerOperand()->stripPointerCasts()))
    return;

  if (!m_graph.hasCell(*LI.getPointerOperand()->stripPointerCasts())) {
    /// XXX: this is very likely because the pointer operand is the
    /// result of applying one or several gep instructions starting
    /// from NULL. Note that this is undefined behavior but it
    /// occurs in ldv benchmarks.
    if (!isa<ConstantExpr>(LI.getPointerOperand()->stripPointerCasts()))
      return;
  }

  /// XXX: special case for casted functions to remove the return value
  /// %3 = load void (i8*, i32*)*,
  ///      void (i8*, i32*)** bitcast (i64 (i8*, i32*)** @listdir to void (i8*,
  ///      i32*)**)
  /// call void %3(i8* %300, i32* %1)
  if (ConstantExpr *CE = dyn_cast<ConstantExpr>(LI.getPointerOperand())) {
    if (CE->isCast() && CE->getOperand(0)->getType()->isPointerTy()) {
      if (GlobalVariable *GV = dyn_cast<GlobalVariable>(CE->getOperand(0))) {
        if (GV->hasInitializer()) {
          if (Function *F = dyn_cast<Function>(GV->getInitializer())) {
            // create cell for LI and record allocation site
            Node &n = m_graph.mkNode();
            sea_dsa::DsaAllocSite *site = m_graph.mkAllocSite(*F);
            assert(site);
            n.addAllocSite(*site);
            m_graph.mkCell(LI, Cell(n, 0));
            return;
          }
        }
      }
    }
  }

  Cell base = valueCell(*LI.getPointerOperand()->stripPointerCasts());
  assert(!base.isNull());
  base.addAccessedType(0, LI.getType());
  base.setRead();
  // update/create the link
  if (!isSkip(LI)) {
    Field LoadedField(0, FieldType(LI.getType()));

    if (!base.hasLink(LoadedField)) {
      Node &n = m_graph.mkNode();
      base.setLink(LoadedField, Cell(&n, 0));
    }

    m_graph.mkCell(LI, base.getLink(LoadedField));
  }

  // handle first-class structs by pretending pointers to them are loaded
  if (isa<StructType>(LI.getType())) {
    Cell dest(base.getNode(), base.getRawOffset());
    m_graph.mkCell(LI, dest);
  }
}

/// OldVal := *Ptr
/// Success := false
/// if OldVal == Cmp then
///    Success := true
///    *Ptr := New
/// return {OldVal, Success}
void IntraBlockBuilder::visitAtomicCmpXchgInst(AtomicCmpXchgInst &I) {
  using namespace sea_dsa;
  
  if (!m_graph.hasCell(*I.getPointerOperand()->stripPointerCasts())) {
    return;
  }
  Value *Ptr = I.getPointerOperand();
  Value *New = I.getNewValOperand();
  
  Cell PtrC = valueCell(*Ptr);
  assert(!PtrC.isNull());

  assert(isa<StructType>(I.getType()));
  Type *ResTy = cast<StructType>(I.getType())->getTypeAtIndex((unsigned) 0); 
  
  PtrC.setModified();
  PtrC.setRead();
  PtrC.growSize(0, Ptr->getType());
  PtrC.addAccessedType(0, Ptr->getType());

  if (ResTy->isPointerTy()) {
    // Load the content of Ptr and make it the cell of the
    // instruction's result.
    Field LoadedField(0, FieldType(ResTy));
    if (!PtrC.hasLink(LoadedField)) {
      Node &n = m_graph.mkNode();
      PtrC.setLink(LoadedField, Cell(n, 0));
    }
    Cell Res = m_graph.mkCell(I, PtrC.getLink(LoadedField));


    if (!isSkip(*New)) {
      Cell NewC = valueCell(*New);
      assert(!NewC.isNull());
      // Merge the result and the content of Ptr with New
      Res.unify(NewC);
    }
  }
}

/// OldVal = *Ptr
/// *Ptr = op(OldVal, Val)
/// return OldVal
void IntraBlockBuilder::visitAtomicRMWInst(AtomicRMWInst &I) {
  Value *Ptr = I.getPointerOperand();

  sea_dsa::Cell PtrC = valueCell(*Ptr);
  assert(!PtrC.isNull());

  PtrC.setModified();
  PtrC.setRead();
  PtrC.growSize(0, I.getType());
  PtrC.addAccessedType(0, I.getType());

  if (!isSkip(I)) {
    sea_dsa::Node &n = m_graph.mkNode();
    sea_dsa::Cell ResC = m_graph.mkCell(I, sea_dsa::Cell(n, 0));
    ResC.unify(PtrC);
  }
}

void IntraBlockBuilder::visitStoreInst(StoreInst &SI) {
  using namespace sea_dsa;

  // -- skip store into NULL
  if (BlockBuilderBase::isNullConstant(
          *SI.getPointerOperand()->stripPointerCasts()))
    return;

  if (!m_graph.hasCell(*SI.getPointerOperand()->stripPointerCasts())) {
    /// XXX: this is very likely because the pointer operand is the
    /// result of applying one or several gep instructions starting
    /// from NULL. Note that this is undefined behavior but it
    /// occurs in ldv benchmarks.
    if (!isa<ConstantExpr>(SI.getPointerOperand()->stripPointerCasts()))
      return;
  }

  Cell base = valueCell(*SI.getPointerOperand()->stripPointerCasts());
  assert(!base.isNull());

  base.setModified();

  Value *ValOp = SI.getValueOperand();
  // XXX: potentially it is enough to update size only at this point
  base.growSize(0, ValOp->getType());
  base.addAccessedType(0, ValOp->getType());

  if (!isSkip(*ValOp)) {
    Cell val = valueCell(*ValOp);

    if (BlockBuilderBase::isNullConstant(*ValOp)) {
      // TODO: mark link as possibly pointing to null
    } else {
      assert(!val.isNull());

      // val.getType() can be an opaque type, so we cannot use it to get
      // a ptr type.
      Cell dest(val.getNode(), val.getRawOffset());
      base.addLink(Field(0, FieldType(ValOp->getType())), dest);
    }
  }
}

void IntraBlockBuilder::visitBitCastInst(BitCastInst &I) {
  if (isSkip(I))
    return;

  if (BlockBuilderBase::isNullConstant(*I.getOperand(0)->stripPointerCasts()))
    return; // do nothing if null

  sea_dsa::Cell arg = valueCell(*I.getOperand(0));
  assert(!arg.isNull());
  m_graph.mkCell(I, arg);
}

/**
   Computes an offset of a gep instruction for a given type and a
   sequence of indicies.

   The first element of the pair is the fixed offset. The second is
   a gcd of the variable offset.
 */
std::pair<int64_t, uint64_t> computeGepOffset(Type *ptrTy,
                                              ArrayRef<Value *> Indicies,
                                              const DataLayout &dl) {
  Type *Ty = ptrTy;
  assert(Ty->isPointerTy());

  // numeric offset
  int64_t noffset = 0;

  // divisor
  uint64_t divisor = 0;

  Type *srcElemTy = cast<PointerType>(ptrTy)->getElementType();
  generic_gep_type_iterator<Value *const *> TI =
      gep_type_begin(srcElemTy, Indicies);

  for (unsigned CurIDX = 0, EndIDX = Indicies.size(); CurIDX != EndIDX;
       ++CurIDX, ++TI) {
    if (StructType *STy = TI.getStructTypeOrNull()) {
      unsigned fieldNo = cast<ConstantInt>(Indicies[CurIDX])->getZExtValue();
      noffset += dl.getStructLayout(STy)->getElementOffset(fieldNo);
      Ty = STy->getElementType(fieldNo);
    } else {
      if (PointerType *ptrTy = dyn_cast<PointerType>(Ty))
        Ty = ptrTy->getElementType();
      else if (SequentialType *seqTy = dyn_cast<SequentialType>(Ty))
        Ty = seqTy->getElementType();
      assert(Ty && "Type is neither PointerType nor SequentialType");

      uint64_t sz = dl.getTypeStoreSize(Ty);
      if (ConstantInt *ci = dyn_cast<ConstantInt>(Indicies[CurIDX])) {
        int64_t arrayIdx = ci->getSExtValue();
        // XXX disabling and handling at the caller
        if (false && arrayIdx < 0) {
          errs() << "WARNING: negative GEP index\n";
          // XXX for now, give up as soon as a negative index is found
          // XXX can probably do better. Some negative indexes are positive
          // offsets
          // XXX others are just moving between array cells
          return std::make_pair(0, 1);
        }
        noffset += (uint64_t)arrayIdx * sz;
      } else
        divisor = divisor == 0 ? sz : gcd(divisor, sz);
    }
  }

  return std::make_pair(noffset, divisor);
}

/// Computes offset into an indexed type
uint64_t computeIndexedOffset(Type *ty, ArrayRef<unsigned> indecies,
                              const DataLayout &dl) {
  uint64_t offset = 0;
  for (unsigned idx : indecies) {
    if (StructType *sty = dyn_cast<StructType>(ty)) {
      const StructLayout *layout = dl.getStructLayout(sty);
      offset += layout->getElementOffset(idx);
      ty = sty->getElementType(idx);
    } else {
      if (PointerType *ptrTy = dyn_cast<PointerType>(ty))
        ty = ptrTy->getElementType();
      else if (SequentialType *seqTy = dyn_cast<SequentialType>(ty))
        ty = seqTy->getElementType();
      assert(ty && "Type is neither PointerType nor SequentialType");
      offset += idx * dl.getTypeAllocSize(ty);
    }
  }
  return offset;
}

void BlockBuilderBase::visitGep(const Value &gep, const Value &ptr,
                                ArrayRef<Value *> indicies) {
  // -- skip NULL
  if (const Constant *c = dyn_cast<Constant>(&ptr))
    if (c->isNullValue()) {
      return;
    }

  /// Special cases in ldv benchmarks
  if (const LoadInst *LI = dyn_cast<LoadInst>(&ptr)) {
    if (BlockBuilderBase::isNullConstant(
            *(LI->getPointerOperand()->stripPointerCasts())))
      return;
  }
  if (const GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(&ptr)) {
    if (BlockBuilderBase::isNullConstant(
            *(GEP->getPointerOperand()->stripPointerCasts())))
      return;
  }

  if (!m_graph.hasCell(ptr) && !isa<GlobalValue>(&ptr)) {
    LOG("dsa", {
      errs() << "Cell not found for gep:\t";
      gep.print(errs());

      if (auto *gepI = dyn_cast<Instruction>(&gep))
        errs() << "\n\t\tin " << gepI->getFunction()->getName() << "\n";

      errs() << "\n\tptr: ";
      ptr.print(errs());
      if (auto *ptrI = dyn_cast<Instruction>(&ptr))
        if (ptrI->getParent())
            errs() << "\n\t\tin " << ptrI->getFunction()->getName() << "\n";
    });
    assert(false && "No cell for gep'd ptr");
    return;
  }

  // -- empty gep that points directly to the base
  if (gep.stripPointerCasts() == &ptr)
    return;

  sea_dsa::Cell base = valueCell(ptr);
  assert(!base.isNull());

  if (m_graph.hasCell(gep)) {
    // gep can have already a cell if it can be stripped to another
    // pointer different from the base or is a constant gep that has already
    // been visited.
    assert(gep.stripPointerCasts() != &gep || llvm::isa<ConstantExpr>(&gep));
    return;
  }

  assert(!m_graph.hasCell(gep));
  sea_dsa::Node *baseNode = base.getNode();
  if (baseNode->isOffsetCollapsed()) {
    m_graph.mkCell(gep, sea_dsa::Cell(baseNode, 0));
    return;
  }

  auto off = computeGepOffset(ptr.getType(), indicies, m_dl);
  if (off.first < 0) {
    if (base.getOffset() + off.first >= 0) {
      m_graph.mkCell(gep,
                     sea_dsa::Cell(*baseNode, base.getOffset() + off.first));
      return;
    } else {
      // create a new node for gep
      sea_dsa::Node &n = m_graph.mkNode();
      // gep points to offset 0
      m_graph.mkCell(gep, sea_dsa::Cell(n, 0));
      // base of gep is at -off.first
      // e.g., off.first is -16, then base is unified at offset 16 with n
      n.unifyAt(*baseNode, -(base.getOffset() + off.first));
      return;
    }
    // XXX This is now unreachable
    //    errs() << "Negative GEP: " << "(" << off.first << ", " << off.second
    //    << ") "
    //           << gep << "\n";
    // XXX current work-around
    // XXX If the offset is negative, convert to an array of stride 1
    LOG("dsa", errs() << "Warning: collapsing negative gep to array: ("
                      << off.first << ", " << off.second << ")\n"
                      << "gep: " << gep << "\n"
                      << "bace cell: " << base << "\n";);
    off = std::make_pair(0, 1);
  }
  if (off.second) {
    // create a node representing the array
    sea_dsa::Node &n = m_graph.mkNode();
    n.setArraySize(off.second);
    // result of the gep points into that array at the gep offset
    // plus the offset of the base
    m_graph.mkCell(gep, sea_dsa::Cell(n, off.first + base.getRawOffset()));
    // finally, unify array with the node of the base
    n.unify(*baseNode);
  } else {
    m_graph.mkCell(gep, sea_dsa::Cell(base, off.first));
  }
}

void IntraBlockBuilder::visitGetElementPtrInst(GetElementPtrInst &I) {
  Value &ptr = *I.getPointerOperand();

  if (isa<ConstantExpr>(ptr)) {
    if (auto *g = dyn_cast<GEPOperator>(&ptr)) {
      // Visit nested constant GEP first.
      SmallVector<Value *, 8> indicies(g->op_begin() + 1, g->op_end());
      visitGep(*g, *g->getPointerOperand(), indicies);
    } else if (auto *bc = dyn_cast<BitCastOperator>(&ptr)) {
      if (auto *g = dyn_cast<GEPOperator>(bc->getOperand(0))) {
        // Visit nested constant GEP first.
        SmallVector<Value *, 8> indicies(g->op_begin() + 1, g->op_end());
        visitGep(*g, *g->getPointerOperand(), indicies);
      }
    }
  }

  if (isa<ShuffleVectorInst>(ptr)) {
    // XXX: TODO: handle properly.
    errs() << "WARNING: Gep inst with a shuffle vector operand "
           << "is allocating a new cell: " << I << "\n";
    m_graph.mkCell(I, sea_dsa::Cell(m_graph.mkNode(), 0));
    return;
  }

  SmallVector<Value *, 8> indicies(I.op_begin() + 1, I.op_end());
  visitGep(I, ptr, indicies);
}

void IntraBlockBuilder::visitInsertValueInst(InsertValueInst &I) {
  assert(I.getAggregateOperand()->getType() == I.getType());
  using namespace sea_dsa;

  // make sure that the aggregate has a cell
  Cell op = valueCell(*I.getAggregateOperand()->stripPointerCasts());
  if (op.isNull()) {
    Node &n = m_graph.mkNode();
    // -- record allocation site
    sea_dsa::DsaAllocSite *site = m_graph.mkAllocSite(I);
    assert(site);
    n.addAllocSite(*site);
    // -- mark node as a stack node
    n.setAlloca();
    // -- create a node for the aggregate
    op = m_graph.mkCell(*I.getAggregateOperand(), Cell(n, 0));
  }

  // -- pretend that the instruction points to the aggregate
  m_graph.mkCell(I, op);

  // -- update type record
  Value &v = *I.getInsertedValueOperand();
  uint64_t offset = computeIndexedOffset(I.getType(), I.getIndices(), m_dl);
  Cell out(op, offset);
  out.growSize(0, v.getType());
  out.addAccessedType(0, v.getType());
  out.setModified();

  // -- update link
  if (!isSkip(v) && !isNullConstant(v)) {
    // TODO: follow valueCell ptrs.
    Cell vCell = valueCell(v);
    assert(!vCell.isNull());
    out.addLink(Field(0, FieldType(v.getType())), vCell);
  }
}

void IntraBlockBuilder::visitExtractValueInst(ExtractValueInst &I) {
  using namespace sea_dsa;
  Cell op = valueCell(*I.getAggregateOperand()->stripPointerCasts());
  if (op.isNull()) {
    Node &n = m_graph.mkNode();
    // -- record allocation site
    sea_dsa::DsaAllocSite *site = m_graph.mkAllocSite(I);
    assert(site);
    n.addAllocSite(*site);
    // -- mark node as a stack node
    n.setAlloca();
    op = m_graph.mkCell(*I.getAggregateOperand(), Cell(n, 0));
  }

  uint64_t offset = computeIndexedOffset(I.getAggregateOperand()->getType(),
                                         I.getIndices(), m_dl);
  FieldType opType(I.getType());
  Cell in(op, offset);

  // -- update type record
  in.addAccessedType(0, I.getType());
  in.setRead();

  if (!isSkip(I)) {
    Field InstType(0, opType);
    // -- create a new node if there is no link at this offset yet
    if (!in.hasLink(InstType)) {
      Node &n = m_graph.mkNode();
      in.setLink(InstType, Cell(&n, 0));
      // -- record allocation site
      sea_dsa::DsaAllocSite *site = m_graph.mkAllocSite(I);
      assert(site);
      n.addAllocSite(*site);
      // -- mark node as a stack node
      n.setAlloca();
    }
    // create cell for the read value and point it to where the link points to
    const Cell &baseC = in.getLink(InstType);
    m_graph.mkCell(I, Cell(baseC.getNode(), baseC.getRawOffset()));
  }
}

void IntraBlockBuilder::visitCallSite(CallSite CS) {
  using namespace sea_dsa;
  Function *callee =
      dyn_cast<Function>(CS.getCalledValue()->stripPointerCasts());
  if (llvm::isAllocationFn(CS.getInstruction(), &m_tli, true) ||
      (callee && m_allocInfo.isAllocWrapper(*callee))) {
    assert(CS.getInstruction());
    Node &n = m_graph.mkNode();
    // -- record allocation site
    sea_dsa::DsaAllocSite *site = m_graph.mkAllocSite(*(CS.getInstruction()));
    assert(site);
    n.addAllocSite(*site);
    // -- mark node as a heap node
    n.setHeap();

    m_graph.mkCell(*CS.getInstruction(), Cell(n, 0));
    return;
  }

  if (callee) {
    /**
        sea_dsa_alias(p1,...,pn)
        unify the cells of p1,...,pn
     **/
    if (isSeaDsaAliasFn(callee)) {
      std::vector<sea_dsa::Cell> toMerge;
      unsigned nargs = CS.arg_size();
      for (unsigned i = 0; i < nargs; ++i) {
        if (isSkip(*(CS.getArgument(i))))
          continue;
        if (!m_graph.hasCell(*(CS.getArgument(i))))
          continue;
        sea_dsa::Cell c = valueCell(*(CS.getArgument(i)));
        if (c.isNull())
          continue;
        toMerge.push_back(c);
      }
      for (unsigned i = 1; i < toMerge.size(); ++i)
        toMerge[0].unify(toMerge[i]);
      return;
    } else if (isSeaDsaCollapseFn(callee)) {
      /**
         sea_dsa_collapse(p)
         collapse the node to which p points to
       **/
      if (!isSkip(*(CS.getArgument(0)))) {
        if (m_graph.hasCell(*(CS.getArgument(0)))) {
          sea_dsa::Cell c = valueCell(*(CS.getArgument(0)));
          c.getNode()->collapseOffsets(__LINE__);
        }
      }
      return;
    } else if (isSeaDsaMkSequenceFn(callee)) {
      /**
         sea_dsa_mk_seq(p, sz)
         mark the node pointed by p as sequence of size sz
       **/
      if (!isSkip(*(CS.getArgument(0)))) {
        if (m_graph.hasCell(*(CS.getArgument(0)))) {
          sea_dsa::Cell c = valueCell(*(CS.getArgument(0)));
          Node *n = c.getNode();
          if (!n->isArray()) {
            ConstantInt *raw_sz = dyn_cast<ConstantInt>(CS.getArgument(1));
            if (!raw_sz) {
              errs() << "WARNING: skipped " << *CS.getInstruction()
                     << " because second argument is not a number.\n";
              return;
            }
            const uint64_t sz = raw_sz->getZExtValue();
            if (n->size() <= sz) {
              n->setArraySize(sz);
            } else {
              errs() << "WARNING: skipped " << *CS.getInstruction()
                     << " because new size cannot be"
                     << " smaller than the size of the node pointed by the "
                        "pointer.\n";
            }
          } else {
            errs() << "WARNING: skipped " << *CS.getInstruction()
                   << " because it expects a pointer"
                   << " that points to a non-sequence node.\n";
          }
        }
      }
      return;
    }
  }

  if (Instruction *inst = CS.getInstruction()) {
    if (!isSkip(*inst)) {
      Cell &c = m_graph.mkCell(*inst, Cell(m_graph.mkNode(), 0));
      if (CS.isInlineAsm()) {
        c.getNode()->setExternal();
        sea_dsa::DsaAllocSite *site = m_graph.mkAllocSite(*inst);
        assert(site);
        c.getNode()->addAllocSite(*site);
      } else if (Function *callee = CS.getCalledFunction()) {
        if (callee->isDeclaration()) {
          c.getNode()->setExternal();
          // -- treat external function as allocation
          // XXX: we ignore external calls created by AbstractMemory pass
          if (!callee->getName().startswith(
                  "verifier.nondet.abstract.memory")) {
            sea_dsa::DsaAllocSite *site = m_graph.mkAllocSite(*inst);
            assert(site);
            c.getNode()->addAllocSite(*site);
          }
          // TODO: many more things can be done to handle external
          // TODO: functions soundly and precisely.  An absolutely
          // safe thing is to merge all arguments with return (with
          // globals) on any external function call. However, this is
          // too aggressive for most cases. More refined analysis can
          // be done using annotations of the external functions (like
          // noalias, does-not-read-memory, etc.). The current
          // solution is okay for now.
        }
      }
    }

    if (m_track_callsites && CS.isIndirectCall()) {
      const Value &calledV = *(CS.getCalledValue());
      if (m_graph.hasCell(calledV)) {
        Cell calledC = m_graph.getCell(calledV);
        m_graph.mkCallSite(*inst, calledC);
      } else {
        errs() << "WARNING: no cell found for callee in indirect call.\n";
      }
    }
  }

  // Value *callee = CS.getCalledValue()->stripPointerCasts();
  // TODO: handle inline assembly
  // TODO: handle variable argument functions
}

void IntraBlockBuilder::visitShuffleVectorInst(ShuffleVectorInst &I) {
  using namespace sea_dsa;

  // XXX: TODO: handle properly.
  errs() << "WARNING: shuffle vector inst is allocating a new cell: " << &I
         << "\n";
  m_graph.mkCell(I, Cell(m_graph.mkNode(), 0));
}

void IntraBlockBuilder::visitMemSetInst(MemSetInst &I) {
  sea_dsa::Cell dest = valueCell(*(I.getDest()));
  // assert (!dest.isNull ());
  if (!dest.isNull())
    dest.setModified();

  // TODO:
  // can also update size using I.getLength ()
}

static bool hasNoPointerTy(const llvm::Type *t) {
  if (!t)
    return true;

  if (t->isPointerTy())
    return false;
  if (const StructType *sty = dyn_cast<const StructType>(t)) {
    for (auto it = sty->element_begin(), end = sty->element_end(); it != end;
         ++it)
      if (!hasNoPointerTy(*it))
        return false;
  } else if (const SequentialType *seqty = dyn_cast<const SequentialType>(t))
    return hasNoPointerTy(seqty->getElementType());

  return true;
}

static bool transfersNoPointers(MemTransferInst &MI, const DataLayout &DL) {
  Value *srcPtr = MI.getSource();
  auto *srcTy = srcPtr->getType()->getPointerElementType();

  ConstantInt *rawLength = dyn_cast<ConstantInt>(MI.getLength());
  if (!rawLength)
    return false;

  const uint64_t length = rawLength->getZExtValue();
  LOG("dsa", errs() << "MemTransfer length:\t" << length << "\n");

  // TODO: Go up to the GEP chain to find nearest fitting type to transfer.
  // This can occur when someone tries to transfer int the middle of a struct.
  if (length * 8 > DL.getTypeSizeInBits(srcTy)) {
    errs() << "WARNING: MemTransfer past object size!\n"
           << "\tTransfer:  ";
    LOG("dsa", MI.print(errs()));
    errs() << "\n\tLength:  " << length
           << "\n\tType size:  " << (DL.getTypeSizeInBits(srcTy) / 8) << "\n";
    return false;
  }

  static SmallDenseMap<std::pair<Type *, unsigned>, bool, 16>
      knownNoPointersInStructs;

  if (knownNoPointersInStructs.count({srcTy, length}) != 0)
    return knownNoPointersInStructs[{srcTy, length}];

  for (auto &subTy : sea_dsa::AggregateIterator::range(srcTy, &DL)) {
    if (subTy.Offset >= length)
      break;

    if (subTy.Ty->isPointerTy()) {
      LOG("dsa", errs() << "Found ptr member "; subTy.Ty->print(errs());
          errs() << "\n\tin "; srcTy->print(errs());
          errs() << "\n\tMemTransfer transfers pointers!\n");

      knownNoPointersInStructs[{srcTy, length}] = false;
      return false;
    }
  }

  knownNoPointersInStructs[{srcTy, length}] = true;
  return true;
}

void IntraBlockBuilder::visitMemTransferInst(MemTransferInst &I) {
  // -- skip copy NULL
  if (isNullConstant(*I.getSource()))
    return;

  // -- skip copy to an unallocated address
  if (isNullConstant(*I.getDest()))
    return;

  if (!m_graph.hasCell(*I.getSource())) {
    errs() << "WARNING: source of memcopy/memmove has no cell: "
           << *I.getSource() << "\n";
    return;
  }

  if (!m_graph.hasCell(*I.getDest())) {
    errs() << "WARNING: destination of memcopy/memmove has no cell: "
           << *I.getDest() << "\n";
    return;
  }

  // -- trust types, i.e., assume types are not abused by storing
  // -- pointers pointers to other types

  bool TrustTypes = true;
  assert(m_graph.hasCell(*I.getDest()));
  assert(m_graph.hasCell(*I.getSource()));

  // unify the two cells because potentially all bytes of source
  // are copied into dest
  sea_dsa::Cell sourceCell = valueCell(*I.getSource());
  sea_dsa::Cell destCell = m_graph.mkCell(*I.getDest(), sea_dsa::Cell());

  if (TrustTypes &&
      ((sourceCell.getNode()->links().size() == 0 &&
        hasNoPointerTy(I.getSource()->getType()->getPointerElementType())) ||
       transfersNoPointers(I, m_dl))) {
    /* do nothing */
    // no pointers are copied from source to dest, so there is no
    // need to unify them
  } else {
    destCell.unify(sourceCell);
  }

  sourceCell.setRead();
  destCell.setModified();

  // TODO: can also update size of dest and source using I.getLength ()
}

namespace {
// -- only used as a compare. do not needs DSA node
bool shouldBeTrackedIntToPtr(const Value &def) {
  // XXX: use_begin will return the same Value def. We need to call
  //      getUser() to get the actual user.
  // if (def.hasOneUse () && isa<CmpInst> (*(def.use_begin ()))) return false;

  if (def.hasNUses(0))
    return false;

  if (def.hasOneUse()) {
    const Value *v = dyn_cast<Value>(def.use_begin()->getUser());
    if (isa<CmpInst>(v))
      return false;

    DenseSet<const Value *> seen;
    while (v && v->hasOneUse() && seen.insert(v).second) {
      if (isa<LoadInst>(v) || isa<StoreInst>(v) || isa<CallInst>(v))
        break;
      v = dyn_cast<const Value>(v->use_begin()->getUser());
    }
    if (isa<CmpInst>(v))
      return false;
  }
  return true;
}

} // namespace

/// Returns true if \p inst is a fixed numeric offset of a known pointer
/// Ensures that \p base has a cell in the current dsa graph
bool BlockBuilderBase::isFixedOffset(const IntToPtrInst &inst, Value *&base,
                                     uint64_t &offset) {
  using namespace llvm::PatternMatch;
  auto *op = inst.getOperand(0);
  Value *X;
  ConstantInt *C;
  if ((match(op, m_Add(m_Value(X), m_ConstantInt(C))) ||
       match(op, m_Add(m_ConstantInt(C), m_Value(X))))) {
    if (match(X, m_PtrToInt(m_Value(base)))) {
      // -- check that source pointer has a cell
      if (!m_graph.hasCell(*base) && !isa<GlobalValue>(base)) {
        LOG("dsa.inttoptr",
            errs() << "Did not find an expected cell for: " << *base << "\n";);
        return false;
      }
      offset = C->getZExtValue();
    } else if (auto *LI = dyn_cast<LoadInst>(X)) {
      PointerType *liType = Type::getInt8PtrTy(LI->getContext());
      sea_dsa::Cell ptrCell =
          valueCell(*LI->getPointerOperand()->stripPointerCasts());
      ptrCell.addAccessedType(0, liType);
      ptrCell.setRead();
      sea_dsa::Field LoadedField(0, sea_dsa::FieldType(liType));
      if (!ptrCell.hasLink(LoadedField)) {
        sea_dsa::Node &n = m_graph.mkNode();
        // XXX: This node will have no allocation site
        ptrCell.setLink(LoadedField, sea_dsa::Cell(&n, 0));
        LOG("dsa.inttoptr",
            errs() << "Created node for LI from external memory without an "
                      "allocation site:\n\tLI: "
                   << *LI << "\n\tbase: "
                   << *LI->getPointerOperand()->stripPointerCasts() << "\n";);
      }
      // create cell for LI
      m_graph.mkCell(*LI, ptrCell.getLink(LoadedField));
      base = LI;
      offset = C->getZExtValue();
    } else {
      return false;
    }
    return true;
  }

  return false;
}

void BlockBuilderBase::visitCastIntToPtr(const Value &dest) {
  // -- only used as a compare. do not needs DSA node
  // if (dest.hasOneUse () && isa<CmpInst> (*(dest.use_begin ()))) return;
  // XXX: we create a new cell for the instruction even if
  //      "shouldBeTrackedIntToPtr(dest)" returns false to avoid
  //      breaking assumptions during the analysis of other
  //      instructions.
  // if (!shouldBeTrackedIntToPtr (dest)) return;
  if (const IntToPtrInst *inttoptr = dyn_cast<IntToPtrInst>(&dest)) {
    Value *base = nullptr;
    uint64_t offset = 0;
    if (isFixedOffset(*inttoptr, base, offset)) {
      // TODO: can extend this to handle variable offsets
      LOG("dsa.inttoptr", errs() << "inttoptr " << dest << "\n\tis " << *base
                                 << " plus " << offset << "\n";);

      if (m_graph.hasCell(*base) || isa<GlobalValue>(base)) {
        sea_dsa::Cell baseCell = valueCell(*base);
        assert(!baseCell.isNull());

        assert(!m_graph.hasCell(dest) && "Untested");
        sea_dsa::Node *baseNode = baseCell.getNode();
        if (baseNode->isOffsetCollapsed())
          m_graph.mkCell(dest, sea_dsa::Cell(baseNode, 0));
        else
          m_graph.mkCell(dest, sea_dsa::Cell(baseCell, offset));
        return;
      }
    }
  }

  sea_dsa::Node &n = m_graph.mkNode();
  n.setIntToPtr();
  // -- record allocation site
  sea_dsa::DsaAllocSite *site = m_graph.mkAllocSite(dest);
  assert(site);
  n.addAllocSite(*site);
  // -- mark node as an alloca node
  n.setAlloca();
  m_graph.mkCell(dest, sea_dsa::Cell(n, 0));
  if (shouldBeTrackedIntToPtr(dest)) {
    if (!m_graph.isFlat()) {
      llvm::errs() << "WARNING: ";
      bool printAddress = true;
      LOG("dsa", llvm::errs() << dest; printAddress = false);
      if (printAddress)
        llvm::errs() << "inttoptr @ addr " << &dest;
      llvm::errs()
          << " is (unsoundly) assumed to point to a fresh memory region.\n";
    }
  }
}

void IntraBlockBuilder::visitIntToPtrInst(IntToPtrInst &I) {
  visitCastIntToPtr(I);
}

void IntraBlockBuilder::visitReturnInst(ReturnInst &RI) {
  Value *v = RI.getReturnValue();

  // We don't skip the return value if its type contains a pointer.
  if (!v || (isSkip(*v) && !containsPointer(*v))) {
    return;
  }

  sea_dsa::Cell c = valueCell(*v);
  if (c.isNull())
    return;

  m_graph.mkRetCell(m_func, c);
}

/**
  Returns true if the given ptrtoint instruction is escaping into
  memory. That is, it flows directly into one of Load/Store/Call
  instructions.

  Load is considered escaping because it is observable from the memory
  perspective
*/
bool isEscapingPtrToInt(const PtrToIntInst &def) {
  SmallVector<const Value *, 16> workList;
  SmallPtrSet<const Value *, 16> seen;
  workList.push_back(&def);

  while (!workList.empty()) {
    const Value *v = workList.back();
    workList.pop_back();
    if (!seen.insert(v).second)
      continue;

    for (auto *user : v->users()) {
      if (seen.count(user))
        continue;
      if (isa<BranchInst>(user) || isa<CmpInst>(user))
        continue;
      if (auto *si = dyn_cast<SelectInst>(user)) {
        if (si->getCondition() == v)
          continue;
      }
      if (auto *bi = dyn_cast<BinaryOperator>(user)) {
        if (bi->getOpcode() == Instruction::Sub &&
            isa<PtrToIntInst>(bi->getOperand(0)) &&
            isa<PtrToIntInst>(bi->getOperand(1)))
          continue;
      }
      if (auto *CI = dyn_cast<const CallInst>(user)) {
        ImmutableCallSite CS(CI);
        const Function *callee = CS.getCalledFunction();
        if (callee && (callee->getName() == "verifier.assume" ||
                       callee->getName() == "llvm.assume"))
          continue;
      }

      // if the value flows into one of these operands, we consider it escaping
      // and stop tracking it further
      if (isa<LoadInst>(*user) || isa<StoreInst>(*user) ||
          isa<CallInst>(*user) || isa<IntToPtrInst>(*user))
        return true;

      workList.push_back(user);
    }
  }
  return false;
}

void IntraBlockBuilder::visitPtrToIntInst(PtrToIntInst &I) {
  if (!isEscapingPtrToInt(I)) 
    return;

  assert(m_graph.hasCell(*I.getOperand(0)));
  sea_dsa::Cell c = valueCell(*I.getOperand(0));
  if (!c.isNull()) {

    // mark node as having a pointer that escapes
    c.getNode()->setPtrToInt(true);

    // print a warning, more verbose under dsa LOG
    LOG("dsa", llvm::errs()
                   << "WARNING: " << I
                   << " might be escaping as is not tracked further\n");
    llvm::errs() << "WARNING: detected ptrtoint instruction that might be "
                    "escaping the analysis. "
                 << "(@" << intptr_t(&I) << ")\n";
  }
}

} // end namespace
namespace sea_dsa {

void LocalAnalysis::runOnFunction(Function &F, Graph &g) {
  LOG("dsa-progress",
      errs() << "Running sea_dsa::Local on " << F.getName() << "\n");
  // create cells and nodes for formal arguments
  for (Argument &a : F.args())
    if (a.getType()->isPointerTy() && !g.hasCell(a)) {
      Node &n = g.mkNode();
      if (TrustArgumentTypes)
        g.mkCell(a, Cell(n, 0));
      else
        g.mkCell(a, Cell(n, 0));
      // -- XXX: hook to record allocation site if F is main
      if (F.getName() == "main") {
        sea_dsa::DsaAllocSite *site = g.mkAllocSite(a);
        assert(site);
        n.addAllocSite(*site);
      }
      // -- mark node as a stack node
      n.setAlloca();
    }

  std::vector<const BasicBlock *> bbs;
  revTopoSort(F, bbs);
  boost::reverse(bbs);

  if (F.getName().equals("main")) {
    GlobalBuilder globalBuilder(F, g, m_dl, m_tli, m_allocInfo);
    globalBuilder.initGlobalVariables();
  }

  IntraBlockBuilder intraBuilder(F, g, m_dl, m_tli, m_allocInfo,
                                 m_track_callsites);
  InterBlockBuilder interBuilder(F, g, m_dl, m_tli, m_allocInfo);
  for (const BasicBlock *bb : bbs)
    intraBuilder.visit(*const_cast<BasicBlock *>(bb));
  for (const BasicBlock *bb : bbs)
    interBuilder.visit(*const_cast<BasicBlock *>(bb));

  g.compress();
  g.remove_dead();

  // clang-format off
  LOG("dsa",
      // --- Sanity check
      for (auto &kv: llvm::make_range(g.scalar_begin(), g.scalar_end()))
        if (kv.second->isRead() || kv.second->isModified())
          if (kv.second->getNode()->getAllocSites().empty()) {
            errs() << "SCALAR " << *(kv.first) << "\n";
            errs() << "WARNING: a node has no allocation site\n";
      } for (auto &kv : llvm::make_range(g.formal_begin(), g.formal_end()))
        if (kv.second->isRead() || kv.second->isModified())
          if (kv.second->getNode()->getAllocSites().empty()) {
            errs() << "FORMAL " << *(kv.first) << "\n";
            errs() << "WARNING: a node has no allocation site\n";
      } for (auto &kv: llvm::make_range(g.return_begin(), g.return_end()))
        if (kv.second->isRead() || kv.second->isModified())
          if (kv.second->getNode()->getAllocSites().empty()) {
            errs() << "RETURN " << kv.first->getName() << "\n";
            errs() << "WARNING: a node has no allocation site\n";
      }
  );
  // clang-format on

  LOG("dsa-local-graph",
      errs() << "### Local Dsa graph after " << F.getName() << "\n";
      g.write(errs()));
}

Local::Local()
    : ModulePass(ID), m_dl(nullptr), m_tli(nullptr), m_allocInfo(nullptr) {}

void Local::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<TargetLibraryInfoWrapperPass>();
  AU.addRequired<AllocWrapInfo>();
  AU.setPreservesAll();
}

bool Local::runOnModule(Module &M) {
  m_dl = &M.getDataLayout();
  m_tli = &getAnalysis<TargetLibraryInfoWrapperPass>().getTLI();
  m_allocInfo = &getAnalysis<AllocWrapInfo>();

  for (Function &F : M)
    runOnFunction(F);
  return false;
}

bool Local::runOnFunction(Function &F) {
  if (F.isDeclaration() || F.empty())
    return false;

  LOG("progress", errs() << "DSA: " << F.getName() << "\n";);

  LocalAnalysis la(*m_dl, *m_tli, *m_allocInfo);
  GraphRef g = std::make_shared<Graph>(*m_dl, m_setFactory);
  la.runOnFunction(F, *g);
  m_graphs[&F] = g;
  return false;
}

bool Local::hasGraph(const Function &F) const {
  return (m_graphs.find(&F) != m_graphs.end());
}

const Graph &Local::getGraph(const Function &F) const {
  auto it = m_graphs.find(&F);
  assert(it != m_graphs.end());
  return *(it->second);
}

// Pass * createDsaLocalPass () {return new Local ();}
} // namespace sea_dsa

char sea_dsa::Local::ID = 0;

static llvm::RegisterPass<sea_dsa::Local>
    X("seadsa-local", "Flow-insensitive, intra-procedural SeaDsa analysis");
