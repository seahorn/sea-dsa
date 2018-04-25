#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/Analysis/CFG.h"
#include "llvm/Analysis/MemoryBuiltins.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/GetElementPtrTypeIterator.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"

#include "sea_dsa/Graph.hh"
#include "sea_dsa/Local.hh"
#include "sea_dsa/support/Debug.h"

#include "boost/range/algorithm/reverse.hpp"

using namespace llvm;

namespace llvm {

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

  bool insertEdge(const BasicBlock *src, const BasicBlock *dst) {
    if (!Visited.isBlockedEdge(src, dst))
      return Visited.insert(dst);

    return false;
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

std::pair<uint64_t, uint64_t>
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

  sea_dsa::Cell valueCell(const Value &v);
  void visitGep(const Value &gep, const Value &base,
                ArrayRef<Value *> indicies);
  void visitCastIntToPtr(const Value &dest);

  bool isSkip(Value &V) {
    if (!V.getType()->isPointerTy())
      return true;
    // XXX skip if only uses are external functions
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
                   const TargetLibraryInfo &tli)
      : m_func(func), m_graph(graph), m_dl(dl), m_tli(tli) {}
};

class InterBlockBuilder : public InstVisitor<InterBlockBuilder>,
                          BlockBuilderBase {
  friend class InstVisitor<InterBlockBuilder>;

  void visitPHINode(PHINode &PHI);

public:
  InterBlockBuilder(Function &func, sea_dsa::Graph &graph, const DataLayout &dl,
                    const TargetLibraryInfo &tli)
      : BlockBuilderBase(func, graph, dl, tli) {}
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

    sea_dsa::Cell c = valueCell(v);
    assert(!c.isNodeNull());
    phi.unify(c);
  }
  assert(!phi.isNodeNull());
}

class IntraBlockBuilder : public InstVisitor<IntraBlockBuilder>,
                          BlockBuilderBase {
  friend class InstVisitor<IntraBlockBuilder>;

  void visitAllocaInst(AllocaInst &AI);
  void visitSelectInst(SelectInst &SI);
  void visitLoadInst(LoadInst &LI);
  void visitStoreInst(StoreInst &SI);
  // void visitAtomicCmpXchgInst(AtomicCmpXchgInst &I);
  // void visitAtomicRMWInst(AtomicRMWInst &I);
  void visitReturnInst(ReturnInst &RI);
  // void visitVAArgInst(VAArgInst   &I);
  void visitIntToPtrInst(IntToPtrInst &I);
  void visitPtrToIntInst(PtrToIntInst &I);
  void visitBitCastInst(BitCastInst &I);
  void visitCmpInst(CmpInst &I) { /* do nothing */
  }
  void visitInsertValueInst(InsertValueInst &I);
  void visitExtractValueInst(ExtractValueInst &I);

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

public:
  IntraBlockBuilder(Function &func, sea_dsa::Graph &graph, const DataLayout &dl,
                    const TargetLibraryInfo &tli)
      : BlockBuilderBase(func, graph, dl, tli) {}
};

sea_dsa::Cell BlockBuilderBase::valueCell(const Value &v) {
  using namespace sea_dsa;
  assert(v.getType()->isPointerTy() || v.getType()->isAggregateType());

  if (isNullConstant(v)) {
    LOG("dsa", errs() << "WARNING: not handled constant: " << v << "\n";);
    return Cell();
  }

  if (m_graph.hasCell(v)) {
    Cell &c = m_graph.mkCell(v, Cell());
    assert(!c.isNodeNull());
    return c;
  }

  if (isa<UndefValue>(&v))
    return Cell();
  if (isa<GlobalAlias>(&v))
    return valueCell(*cast<GlobalAlias>(&v)->getAliasee());

  if (isa<ConstantStruct>(&v) || isa<ConstantArray>(&v) ||
      isa<ConstantDataSequential>(&v) || isa<ConstantDataArray>(&v) ||
      isa<ConstantDataVector>(&v)) {
    // XXX Handle properly
    assert(false);
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
      Value &i = *(ce->getOperand(0));
      visitCastIntToPtr(i);
      assert(m_graph.hasCell(i));
      return m_graph.mkCell(i, Cell());
    }
  }

  errs() << v << "\n";
  assert(false && "Not handled expression");
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
  n.addAllocSite(AI);
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

  Cell base = valueCell(*LI.getPointerOperand()->stripPointerCasts());
  assert(!base.isNodeNull());
  base.addAccessedType(0, LI.getType());
  base.setRead();
  // update/create the link
  if (!isSkip(LI)) {
    if (!base.hasLink()) {
      Node &n = m_graph.mkNode();
      base.setLink(0, Cell(&n, 0));
    }
    m_graph.mkCell(LI, base.getLink());
  }

  // handle first-class structs by pretending pointers to them are loaded
  if (isa<StructType>(LI.getType())) {
    Cell dest(base.getNode(), base.getRawOffset());
    m_graph.mkCell(LI, dest);
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
  assert(!base.isNodeNull());

  base.setModified();

  // XXX: potentially it is enough to update size only at this point
  base.growSize(0, SI.getValueOperand()->getType());
  base.addAccessedType(0, SI.getValueOperand()->getType());

  if (!isSkip(*SI.getValueOperand())) {
    Cell val = valueCell(*SI.getValueOperand());
    if (BlockBuilderBase::isNullConstant(*SI.getValueOperand())) {
      // TODO: mark link as possibly pointing to null
    } else {
      assert(!val.isNodeNull());
      base.addLink(0, val);
    }
  }
}

void IntraBlockBuilder::visitBitCastInst(BitCastInst &I) {
  if (isSkip(I))
    return;

  if (BlockBuilderBase::isNullConstant(*I.getOperand(0)->stripPointerCasts()))
    return; // do nothing if null

  sea_dsa::Cell arg = valueCell(*I.getOperand(0));
  assert(!arg.isNodeNull());
  m_graph.mkCell(I, arg);
}

/**
   Computes an offset of a gep instruction for a given type and a
   sequence of indicies.

   The first element of the pair is the fixed offset. The second is
   a gcd of the variable offset.
 */
std::pair<uint64_t, uint64_t> computeGepOffset(Type *ptrTy,
                                               ArrayRef<Value *> Indicies,
                                               const DataLayout &dl) {
  Type *Ty = ptrTy;
  assert(Ty->isPointerTy());

  // numeric offset
  uint64_t noffset = 0;

  // divisor
  uint64_t divisor = 0;

  generic_gep_type_iterator<Value *const *> TI =
      gep_type_begin(ptrTy, Indicies);
  for (unsigned CurIDX = 0, EndIDX = Indicies.size(); CurIDX != EndIDX;
       ++CurIDX, ++TI) {
    if (StructType *STy = dyn_cast<StructType>(*TI)) {
      unsigned fieldNo = cast<ConstantInt>(Indicies[CurIDX])->getZExtValue();
      noffset += dl.getStructLayout(STy)->getElementOffset(fieldNo);
      Ty = STy->getElementType(fieldNo);
    } else {
      Ty = cast<SequentialType>(Ty)->getElementType();
      uint64_t sz = dl.getTypeStoreSize(Ty);
      if (ConstantInt *ci = dyn_cast<ConstantInt>(Indicies[CurIDX])) {
        int64_t arrayIdx = ci->getSExtValue();
        if (arrayIdx < 0) {
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
      ty = cast<SequentialType>(ty)->getElementType();
      offset += idx * dl.getTypeAllocSize(ty);
    }
  }
  return offset;
}

void BlockBuilderBase::visitGep(const Value &gep, const Value &ptr,
                                ArrayRef<Value *> indicies) {
  // -- skip NULL
  if (const Constant *c = dyn_cast<Constant>(&ptr))
    if (c->isNullValue())
      return;

  // -- skip NULL
  if (const LoadInst *LI = dyn_cast<LoadInst>(&ptr)) {
    /// XXX: this occurs in several ldv benchmarks
    if (BlockBuilderBase::isNullConstant(
            *(LI->getPointerOperand()->stripPointerCasts())))
      return;
  }

  assert(m_graph.hasCell(ptr) || isa<GlobalValue>(&ptr));

  // -- empty gep that points directly to the base
  if (gep.stripPointerCasts() == &ptr)
    return;

  sea_dsa::Cell base = valueCell(ptr);
  assert(!base.isNodeNull());

  if (m_graph.hasCell(gep)) {
    // gep can have already a cell if it can be stripped to another
    // pointer different from the base.
    if (gep.stripPointerCasts() != &gep)
      return;
  }

  assert(!m_graph.hasCell(gep));
  sea_dsa::Node *baseNode = base.getNode();
  if (baseNode->isCollapsed()) {
    m_graph.mkCell(gep, sea_dsa::Cell(baseNode, 0));
    return;
  }

  auto off = computeGepOffset(ptr.getType(), indicies, m_dl);
  if (off.second) {
    // create a node representing the array
    sea_dsa::Node &n = m_graph.mkNode();
    n.setArraySize(off.second);
    // result of the gep points into that array at the gep offset
    // plus the offset of the base
    m_graph.mkCell(gep, sea_dsa::Cell(n, off.first + base.getRawOffset()));
    // finally, unify array with the node of the base
    n.unify(*baseNode);
  } else
    m_graph.mkCell(gep, sea_dsa::Cell(base, off.first));
}

void IntraBlockBuilder::visitGetElementPtrInst(GetElementPtrInst &I) {
  Value &ptr = *I.getPointerOperand();
  SmallVector<Value *, 8> indicies(I.op_begin() + 1, I.op_end());
  visitGep(I, ptr, indicies);
}

void IntraBlockBuilder::visitInsertValueInst(InsertValueInst &I) {
  assert(I.getAggregateOperand()->getType() == I.getType());
  using namespace sea_dsa;

  // make sure that the aggregate has a cell
  Cell op = valueCell(*I.getAggregateOperand()->stripPointerCasts());
  if (op.isNodeNull()) {
    Node &n = m_graph.mkNode();
    // -- record allocation site
    n.addAllocSite(I);
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
  if (!isSkip(v)) {
    Cell vCell = valueCell(v);
    assert(!vCell.isNodeNull());
    out.addLink(0, vCell);
  }
}

void IntraBlockBuilder::visitExtractValueInst(ExtractValueInst &I) {
  using namespace sea_dsa;
  Cell op = valueCell(*I.getAggregateOperand()->stripPointerCasts());
  if (op.isNodeNull()) {
    Node &n = m_graph.mkNode();
    // -- record allocation site
    n.addAllocSite(I);
    // -- mark node as a stack node
    n.setAlloca();
    op = m_graph.mkCell(*I.getAggregateOperand(), Cell(n, 0));
  }

  uint64_t offset = computeIndexedOffset(I.getAggregateOperand()->getType(),
                                         I.getIndices(), m_dl);
  Cell in(op, offset);

  // -- update type record
  in.addAccessedType(0, I.getType());
  in.setRead();

  if (!isSkip(I)) {
    // -- create a new node if there is no link at this offset yet
    if (!in.hasLink()) {
      Node &n = m_graph.mkNode();
      in.setLink(0, Cell(&n, 0));
      // -- record allocation site
      n.addAllocSite(I);
      // -- mark node as a stack node
      n.setAlloca();
    }
    // create cell for the read value and point it to where the link points to
    m_graph.mkCell(I, in.getLink());
  }
}

void IntraBlockBuilder::visitCallSite(CallSite CS) {
  using namespace sea_dsa;

  if (llvm::isAllocationFn(CS.getInstruction(), &m_tli, true)) {
    assert(CS.getInstruction());
    Node &n = m_graph.mkNode();
    // -- record allocation site
    n.addAllocSite(*(CS.getInstruction()));
    // -- mark node as a heap node
    n.setHeap();
    m_graph.mkCell(*CS.getInstruction(), Cell(n, 0));
    return;
  }

  if (const Function *callee =
          dyn_cast<Function>(CS.getCalledValue()->stripPointerCasts())) {
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
        if (c.isNodeNull())
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
          c.getNode()->collapse(__LINE__);
        }
      }
      return;
    }
  }

  Instruction *inst = CS.getInstruction();
  if (inst && !isSkip(*inst)) {
    Cell &c = m_graph.mkCell(*inst, Cell(m_graph.mkNode(), 0));
    if (Function *callee = CS.getCalledFunction()) {
      if (callee->isDeclaration()) {
        c.getNode()->setExternal();
        // -- treat external function as allocation
        // XXX: we ignore external calls created by AbstractMemory pass
        if (!callee->getName().startswith("verifier.nondet.abstract.memory"))
          c.getNode()->addAllocSite(*inst);

        // TODO: many more things can be done to handle external
        // TODO: functions soundly and precisely.  An absolutely
        // safe thing is to merge all arguments with return (with
        // globals) on any external function call. However, this is
        // too aggressive for most cases. More refined analysis can
        // be done using annotations of the external functions (like
        // noalias, does-not-read-memory, etc.). The current
        // solution is okay for now.
      }
    } else {
      // TODO: handle indirect call
    }
  }

  Value *callee = CS.getCalledValue()->stripPointerCasts();
  // TODO: handle inline assembly
  // TODO: handle variable argument functions
}

void IntraBlockBuilder::visitMemSetInst(MemSetInst &I) {
  sea_dsa::Cell dest = valueCell(*(I.getDest()));
  // assert (!dest.isNodeNull ());
  if (!dest.isNodeNull())
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

struct SubTypeDesc {
  Type *Ty = nullptr;
  unsigned Bytes = 0;
  unsigned Offset = 0;

  SubTypeDesc() = default;
  SubTypeDesc(Type *Ty_, unsigned Bytes_, unsigned Offset_)
      : Ty(Ty_), Bytes(Bytes_), Offset(Offset_) {}

  void dump(raw_ostream &OS = llvm::errs()) {
    OS << "SubTypeDesc:  ";
    if (Ty)
      Ty->print(OS);
    else
      OS << "nullptr";

    OS << "\n\tbytes:  " << Bytes << "\n\toffset:  " << Offset << "\n";
  }
};

class AggregateIterator {
  const DataLayout *DL;
  SmallVector<Type *, 8> Worklist;
  SubTypeDesc Current;

  AggregateIterator(const DataLayout &DL, Type *Ty) : DL(&DL) {
    Worklist.push_back({Ty});
  }

public:
  static AggregateIterator mkBegin(Type *Ty, const DataLayout &DL) {
    AggregateIterator Res(DL, Ty);
    Res.doStep();
    return Res;
  }

  static AggregateIterator mkEnd(Type *Ty, const DataLayout &DL) {
    AggregateIterator Res(DL, nullptr);
    Res.Worklist.clear();
    return Res;
  }

  static llvm::iterator_range<AggregateIterator> range(Type *Ty,
                                                       const DataLayout &DL) {
    return llvm::make_range(mkBegin(Ty, DL), mkEnd(Ty, DL));
  }

  AggregateIterator &operator++() {
    doStep();
    return *this;
  }

  SubTypeDesc &operator*() { return Current; }
  SubTypeDesc *operator->() { return &Current; }

  bool operator!=(const AggregateIterator &Other) const {
    return Current.Ty != Other.Current.Ty ||
           Current.Bytes != Other.Current.Bytes || Worklist != Other.Worklist;
  }

  unsigned sizeInBytes(Type *Ty) const {
    const auto Bits = DL->getTypeSizeInBits(Ty);
    assert(Bits >= 8);
    return Bits / 8;
  }

private:
  void doStep() {
    if (Worklist.empty()) {
      const auto NewOffset = Current.Offset + Current.Bytes;
      Current = {nullptr, 0, NewOffset};
      return;
    }

    while (!Worklist.empty()) {
      auto *const Ty = Worklist.pop_back_val();

      if (Ty->isPointerTy() || !isa<CompositeType>(Ty)) {
        const auto NewOffset = Current.Offset + Current.Bytes;
        Current = SubTypeDesc{Ty, sizeInBytes(Ty), NewOffset};
        break;
      }

      if (Ty->isArrayTy()) {
        for (size_t i = 0, e = Ty->getArrayNumElements(); i != e; ++i)
          Worklist.push_back(Ty->getArrayElementType());
        continue;
      }

      if (Ty->isVectorTy()) {
        for (size_t i = 0, e = Ty->getVectorNumElements(); i != e; ++i)
          Worklist.push_back(Ty->getVectorElementType());
        continue;
      }

      assert(Ty->isStructTy());
      for (auto *SubTy : llvm::reverse(Ty->subtypes()))
        Worklist.push_back(SubTy);
    }
  }
};

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
    MI.print(errs());
    errs() << "\n\tLength:  " << length
           << "\n\tType size:  " << (DL.getTypeSizeInBits(srcTy) / 8) << "\n";
    return false;
  }

  static SmallDenseMap<std::pair<Type *, unsigned>, bool, 16>
      knownNoPointersInStructs;

  if (knownNoPointersInStructs.count({srcTy, length}) != 0)
    return knownNoPointersInStructs[{srcTy, length}];

  for (auto &subTy : AggregateIterator::range(srcTy, DL)) {
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

// -- only used as a compare. do not needs DSA node
bool shouldBeTrackedIntToPtr(const Value &def) {
  // XXX: use_begin will return the same Value def. We need to call
  //      getUser() to get the actual user.
  // if (def.hasOneUse () && isa<CmpInst> (*(def.use_begin ()))) return false;

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

void BlockBuilderBase::visitCastIntToPtr(const Value &dest) {
  // -- only used as a compare. do not needs DSA node
  // if (dest.hasOneUse () && isa<CmpInst> (*(dest.use_begin ()))) return;
  // XXX: we create a new cell for the instruction even if
  //      "shouldBeTrackedIntToPtr(dest)" returns false to avoid
  //      breaking assumptions during the analysis of other
  //      instructions.
  // if (!shouldBeTrackedIntToPtr (dest)) return;

  sea_dsa::Node &n = m_graph.mkNode();
  n.setIntToPtr();
  // -- record allocation site
  n.addAllocSite(dest);
  // -- mark node as an alloca node
  n.setAlloca();
  m_graph.mkCell(dest, sea_dsa::Cell(n, 0));
  if (shouldBeTrackedIntToPtr(dest)) {
    llvm::errs() << "WARNING: " << dest << " is allocating a new cell. "
                 << "It might be unsound if not flat memory model.\n";
  }
}

void IntraBlockBuilder::visitIntToPtrInst(IntToPtrInst &I) {
  visitCastIntToPtr(I);
}

void IntraBlockBuilder::visitReturnInst(ReturnInst &RI) {
  Value *v = RI.getReturnValue();
  if (!v || isSkip(*v))
    return;

  sea_dsa::Cell c = valueCell(*v);
  if (c.isNodeNull())
    return;

  m_graph.mkRetCell(m_func, c);
}

bool shouldBeTrackedPtrToInt(const Value &def) {
  if (def.hasOneUse() && isa<CmpInst>(*(def.use_begin()->getUser())))
    return false;

  if (def.hasOneUse()) {
    Value *v = dyn_cast<Value>(def.use_begin()->getUser());
    DenseSet<Value *> seen;
    while (v && v->hasOneUse() && seen.insert(v).second) {
      if (isa<LoadInst>(v) || isa<StoreInst>(v) || isa<CallInst>(v))
        break;
      v = dyn_cast<Value>(v->use_begin()->getUser());
    }
    if (isa<BranchInst>(v))
      return false;

    /// XXX: search for a common pattern in which all uses are
    /// assume functions
    if (!v->hasOneUse()) {
      for (auto const &U : v->uses()) {
        if (const CallInst *CI = dyn_cast<const CallInst>(U.getUser())) {
          ImmutableCallSite CS(CI);
          const Function *callee = CS.getCalledFunction();
          if (callee && (callee->getName() == "verifier.assume" ||
                         callee->getName() == "llvm.assume"))
            continue;
        }
        return true;
      }
      return false;
    }
  }
  return true;
}

void IntraBlockBuilder::visitPtrToIntInst(PtrToIntInst &I) {
  if (!shouldBeTrackedPtrToInt(I))
    return;

  assert(m_graph.hasCell(*I.getOperand(0)));
  sea_dsa::Cell c = valueCell(*I.getOperand(0));
  if (!c.isNodeNull()) {
    llvm::errs() << "WARNING: " << I << " may be escaping.\n";
    c.getNode()->setPtrToInt();
  }
}

} // end namespace

namespace sea_dsa {

void LocalAnalysis::runOnFunction(Function &F, Graph &g) {
  // create cells and nodes for formal arguments
  for (Argument &a : F.args())
    if (a.getType()->isPointerTy() && !g.hasCell(a)) {
      Node &n = g.mkNode();
      g.mkCell(a, Cell(n, 0));
      // -- XXX: hook to record allocation site if F is main
      if (F.getName() == "main")
        n.addAllocSite(a);
      // -- mark node as a stack node
      n.setAlloca();
    }

  std::vector<const BasicBlock *> bbs;
  revTopoSort(F, bbs);
  boost::reverse(bbs);

  IntraBlockBuilder intraBuilder(F, g, m_dl, m_tli);
  InterBlockBuilder interBuilder(F, g, m_dl, m_tli);
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

Local::Local() : ModulePass(ID), m_dl(nullptr), m_tli(nullptr) {}

void Local::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<TargetLibraryInfoWrapperPass>();
  AU.setPreservesAll();
}

bool Local::runOnModule(Module &M) {
  m_dl = &M.getDataLayout();
  m_tli = &getAnalysis<TargetLibraryInfoWrapperPass>().getTLI();

  for (Function &F : M)
    runOnFunction(F);
  return false;
}

bool Local::runOnFunction(Function &F) {
  if (F.isDeclaration() || F.empty())
    return false;

  LOG("progress", errs() << "DSA: " << F.getName() << "\n";);

  LocalAnalysis la(*m_dl, *m_tli);
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
    X("seadsa-local", "Flow-insensitive, intra-procedural dsa analysis");
