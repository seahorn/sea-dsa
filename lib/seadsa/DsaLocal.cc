//===- Local Alias Analysis
//
// Part of the SeaHorn Project, under the modified BSD Licence with SeaHorn
// exceptions.
//
// See LICENSE.txt for license information.
//
//===----------------------------------------------------------------------===//
//
// This file contains the LOCAL step of SeaDsa Alias Analysis
//
//===----------------------------------------------------------------------===//

#include "seadsa/Local.hh"

#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/Analysis/CFG.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/MemoryBuiltins.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/GetElementPtrTypeIterator.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/PatternMatch.h"

#include "seadsa/AllocWrapInfo.hh"
#include "seadsa/Graph.hh"
#include "seadsa/TypeUtils.hh"
#include "seadsa/support/Debug.h"

#include "boost/range/algorithm/reverse.hpp"

using namespace llvm;

static llvm::cl::opt<bool> TrustArgumentTypes(
    "sea-dsa-trust-args",
    llvm::cl::desc("Trust function argument types in SeaDsa Local analysis"),
    llvm::cl::init(true));
static llvm::cl::opt<bool> AssumeExternalFunctonsAllocators(
    "sea-dsa-assume-external-functions-allocators",
    llvm::cl::desc(
        "Treat all external functions as potential memory allocators"),
    llvm::cl::init(false));

/*****************************************************************************/
/* HELPERS                                                                   */
/*****************************************************************************/

namespace {
/**
   adapted from SeaHorn

   Keeps track of pair of basic blocks and reports a saved pair as blocked
**/
class BlockedEdges {
public:
  using BasicBlockPtrSet = llvm::SmallPtrSet<const BasicBlock *, 8>;
  using BasicBlockPtrPair = std::pair<const BasicBlock *, const BasicBlock *>;
  using BasicBlockPtrPairVec = llvm::SmallVectorImpl<BasicBlockPtrPair>;

private:
  BasicBlockPtrSet m_set;
  BasicBlockPtrPairVec &m_edges;
  bool m_dirty;

public:
  BlockedEdges(BasicBlockPtrPairVec &edges) : m_edges(edges), m_dirty(true) {
    std::sort(m_edges.begin(), m_edges.end());
    m_dirty = false;
  }

  bool insert(const BasicBlock *bb) {
    return m_set.insert(bb).second;
    m_dirty = true;
  }

  bool isBlockedEdge(const BasicBlock *src, const BasicBlock *dst) {
    if (m_dirty) std::stable_sort(m_edges.begin(), m_edges.end());
    m_dirty = false;
    return std::binary_search(m_edges.begin(), m_edges.end(),
                              std::make_pair(src, dst));
  }
};
} // namespace

namespace llvm {
template <> class po_iterator_storage<BlockedEdges, true> {
  BlockedEdges &Visited;

public:
  po_iterator_storage(BlockedEdges &VSet) : Visited(VSet) {}
  po_iterator_storage(const po_iterator_storage &S) : Visited(S.Visited) {}

  bool insertEdge(Optional<const BasicBlock *> src, const BasicBlock *dst) {
    return Visited.insert(dst);
  }
  void finishPostorder(const BasicBlock *bb) {}
};
} // namespace llvm

namespace {
/* borrowed from seahorn
   Reverse topological sort of (possibly cyclic) CFG
 */
template <typename Container>
void revTopoSort(const llvm::Function &F, Container &out) {
  if (F.isDeclaration() || F.empty()) return;

  llvm::SmallVector<BlockedEdges::BasicBlockPtrPair, 8> backEdges;
  FindFunctionBackedges(F, backEdges);

  auto *f = &F;
  BlockedEdges ble(backEdges);
  std::copy(po_ext_begin(f, ble), po_ext_end(f, ble), std::back_inserter(out));
}

/// Work-arround for a bug in llvm::CallBase::getCalledFunction
/// properly handle bitcast
Function *getCalledFunction(CallBase &CB) {
  Function *fn = CB.getCalledFunction();
  if (fn) return fn;

  Value *v = CB.getCalledOperand();
  if (v) v = v->stripPointerCasts();
  fn = dyn_cast<Function>(v);

  return fn;
}

} // namespace

/*****************************************************************************/
/* ANALYSIS                                                                  */
/* The analysis is broken into 3 phases, each represented by a separate class
     - GlobalBuilder
     - IntraBlockBuilder
     - InterBlockBuilder

   Common functionality of the Builders is factored to BlockBuilderBase.

   A builder visits instructions and constructs a seadsa::Graph representing
   their points-to graph.

   GlobalBuilder processes all the global instructions (i.e., global variable
   declarations)

   IntraBlockBuilder processes all instructions of a basic block, except for
   PHINodes

   InterBlockBuilder processes all the PHINode instructions and combines
   points-to information from basic blocks together. It must be ran after
   IntraBlockBuilder

 */
/*****************************************************************************/

namespace {
// forward declaration
std::pair<int64_t, uint64_t>
computeGepOffset(Type *ptrTy, ArrayRef<Value *> Indicies, const DataLayout &dl);

/*****************************************************************************/
/* BlockBuilderBase */
/*****************************************************************************/
/// Base class for all Builders
class BlockBuilderBase {
protected:
  /// Current function
  Function &m_func;
  /// Current points-to graph
  seadsa::Graph &m_graph;

  const DataLayout &m_dl;
  const TargetLibraryInfo &m_tli;
  const seadsa::AllocWrapInfo &m_allocInfo;

  /// Returns cell for a given llvm::Value
  seadsa::Cell valueCell(const llvm::Value &v);

  void visitGep(const Value &gep, const Value &base,
                ArrayRef<Value *> indicies);
  void visitCastIntToPtr(const Value &dest);

  bool isFixedOffset(const IntToPtrInst &inst, Value *&base, uint64_t &offset);

  /// Returns true if the given llvm::Value should not be analyzed
  bool isSkip(Value &V) {
    if (V.getType()->isPointerTy()) { return false; }
    if (isa<PtrToIntInst>(V)) { return false; }

    return true;
  }

  /// Returns true if \p _Ty has a pointer field
  /// If \p _Ty is not an aggregate, returns true if \p _Ty is a pointer
  static bool hasPointerTy(const llvm::Type &_Ty) {
    SmallVector<const Type *, 16> workList;
    SmallPtrSet<const Type *, 16> seen;
    workList.push_back(&_Ty);

    while (!workList.empty()) {
      const Type *Ty = workList.back();
      workList.pop_back();
      if (!seen.insert(Ty).second) continue;

      if (Ty->isPointerTy()) { return true; }

      if (const StructType *ST = dyn_cast<StructType>(Ty)) {
        for (unsigned i = 0, sz = ST->getNumElements(); i < sz; ++i) {
          workList.push_back(ST->getElementType(i));
        }
      } else if (Ty->isArrayTy()) {
        workList.push_back(Ty->getArrayElementType());
      } else if (auto vt = dyn_cast<VectorType>(Ty)) {
        workList.push_back(vt->getElementType());
      }
    }
    return false;
  }
  /// Returns true if type of \p V contains a pointer
  static bool containsPointer(const Value &V) {
    return hasPointerTy(*V.getType());
  }

  static bool isNullConstant(const Value &v) {
    const Value *V = v.stripPointerCasts();

    if (isa<Constant>(V) && cast<Constant>(V)->isNullValue()) return true;

    // TODO: check whether v can simplify to a NullValue

    if (auto *cast = dyn_cast<BitCastInst>(&v)) {
      assert(cast->getOperand(0));
      // recursion is bounded by levels of bitcasts
      return isNullConstant(*cast->getOperand(0));
    } else if (auto *cast = dyn_cast<PtrToIntInst>(&v)) {
      assert(cast->getOperand(0));
      return isNullConstant(*cast->getOperand(0));
    }
    // Some LDV examples in SV-COMP benchmarks might contain gep null, ....
    else if (const GetElementPtrInst *Gep =
                 dyn_cast<const GetElementPtrInst>(V)) {
      const Value &base = *Gep->getPointerOperand();
      if (const Constant *c = dyn_cast<Constant>(base.stripPointerCasts()))
        return c->isNullValue();
    }

    return false;
  }

public:
  BlockBuilderBase(Function &func, seadsa::Graph &graph, const DataLayout &dl,
                   const TargetLibraryInfo &tli,
                   const seadsa::AllocWrapInfo &allocInfo)
      : m_func(func), m_graph(graph), m_dl(dl), m_tli(tli),
        m_allocInfo(allocInfo) {}
};

/*****************************************************************************/
/* GlobalBuilder */
/*****************************************************************************/
class GlobalBuilder : public BlockBuilderBase {

  /// from: llvm/lib/ExecutionEngine/ExecutionEngine.cpp
  void init(const Constant *Init, seadsa::Cell &c, unsigned offset) {
    if (isa<UndefValue>(Init)) return;

    if (const ConstantVector *CP = dyn_cast<ConstantVector>(Init)) {
      unsigned ElementSize =
          m_dl.getTypeAllocSize(CP->getType()->getElementType()).getFixedSize();
      for (unsigned i = 0, e = CP->getNumOperands(); i != e; ++i) {
        unsigned noffset = offset + i * ElementSize;
        seadsa::Cell nc = seadsa::Cell(c.getNode(), noffset);
        init(CP->getOperand(i), nc, noffset);
      }
      return;
    }

    if (isa<ConstantAggregateZero>(Init)) { return; }

    if (const ConstantArray *CPA = dyn_cast<ConstantArray>(Init)) {
      unsigned ElementSize =
          m_dl.getTypeAllocSize(CPA->getType()->getElementType())
              .getFixedSize();
      for (unsigned i = 0, e = CPA->getNumOperands(); i != e; ++i) {
        unsigned noffset = offset + i * ElementSize;
        seadsa::Cell nc = seadsa::Cell(c.getNode(), noffset);
        init(CPA->getOperand(i), nc, noffset);
      }
      return;
    }

    if (const ConstantStruct *CPS = dyn_cast<ConstantStruct>(Init)) {
      const StructLayout *SL =
          m_dl.getStructLayout(cast<StructType>(CPS->getType()));
      for (unsigned i = 0, e = CPS->getNumOperands(); i != e; ++i) {
        unsigned noffset = offset + SL->getElementOffset(i);
        seadsa::Cell nc = seadsa::Cell(c.getNode(), noffset);
        init(CPS->getOperand(i), nc, noffset);
      }
      return;
    }

    if (isa<ConstantDataSequential>(Init)) { return; }

    if (Init->getType()->isPointerTy() && !isa<ConstantPointerNull>(Init)) {
      if (cast<PointerType>(Init->getType())
              ->getElementType()
              ->isFunctionTy()) {
        seadsa::Node &n = m_graph.mkNode();
        seadsa::Cell nc(n, 0);
        seadsa::DsaAllocSite *site = m_graph.mkAllocSite(*Init);
        assert(site);
        n.addAllocSite(*site);

        // connect c with nc
        c.growSize(0, Init->getType());
        c.addAccessedType(0, Init->getType());
        c.addLink(seadsa::Field(0, seadsa::FieldType(Init->getType())), nc);
        return;
      }

      if (m_graph.hasCell(*Init)) {
        // @g1 =  ...*
        // @g2 =  ...** @g1
        seadsa::Cell &nc = m_graph.mkCell(*Init, seadsa::Cell());
        // connect c with nc
        c.growSize(0, Init->getType());
        c.addAccessedType(0, Init->getType());
        c.addLink(seadsa::Field(0, seadsa::FieldType(Init->getType())), nc);
        return;
      }
    }
  }

public:
  // XXX: it should take a module but we want to reuse
  // BlockBuilderBase so we need to pass a function. We assume that
  // the passed function is main.
  GlobalBuilder(Function &func, seadsa::Graph &graph, const DataLayout &dl,
                const TargetLibraryInfo &tli,
                const seadsa::AllocWrapInfo &allocInfo)
      : BlockBuilderBase(func, graph, dl, tli, allocInfo) {}

  void initGlobalVariables() {
    if (!m_func.getName().equals("main")) return;

    Module &M = *(m_func.getParent());
    for (auto &gv : M.globals()) {
      if (gv.getName().equals("llvm.used")) continue;

      if (gv.hasInitializer()) {
        seadsa::Cell c = valueCell(gv);
        init(gv.getInitializer(), c, 0);
      }
    }
  }
};

/*****************************************************************************/
/* InterBlockBuilder */
/*****************************************************************************/
class InterBlockBuilder : public InstVisitor<InterBlockBuilder>,
                          BlockBuilderBase {
  friend class InstVisitor<InterBlockBuilder>;

  void visitPHINode(PHINode &PHI);

public:
  InterBlockBuilder(Function &func, seadsa::Graph &graph, const DataLayout &dl,
                    const TargetLibraryInfo &tli,
                    const seadsa::AllocWrapInfo &allocInfo)
      : BlockBuilderBase(func, graph, dl, tli, allocInfo) {}
};

void InterBlockBuilder::visitPHINode(PHINode &PHI) {
  if (!PHI.getType()->isPointerTy()) return;

  assert(m_graph.hasCell(PHI));
  seadsa::Cell &phi = m_graph.mkCell(PHI, seadsa::Cell());
  for (unsigned i = 0, e = PHI.getNumIncomingValues(); i < e; ++i) {
    Value &v = *PHI.getIncomingValue(i);

    // -- skip null
    if (BlockBuilderBase::isNullConstant(v)) continue;

    // -- skip load of null
    if (auto *LI = dyn_cast<LoadInst>(&v)) {
      if (BlockBuilderBase::isNullConstant(*LI->getPointerOperand())) continue;
    }

    // -- skip undef
    if (isa<Constant>(&v) && isa<UndefValue>(&v)) continue;

    // --- real code begins ---
    seadsa::Cell c = valueCell(v);
    if (c.isNull()) {
      // -- skip null: special case from ldv benchmarks
      if (auto *GEP = dyn_cast<GetElementPtrInst>(&v)) {
        if (BlockBuilderBase::isNullConstant(*GEP->getPointerOperand()))
          continue;
      }
      assert(false && "Unexpected null cell");
    }

    phi.unify(c);
  }
  assert(!phi.isNull());
}

enum class SeadsaFn {
  MODIFY,
  READ,
  HEAP,
  PTR_TO_INT,
  INT_TO_PTR,
  EXTERN,
  ALIAS,
  COLLAPSE,
  MAKE_SEQ,
  LINK,
  MAKE,
  ACCESS,
  UNKNOWN
};

/*****************************************************************************/
/* IntraBlockBuilder */
/*****************************************************************************/
class IntraBlockBuilder : public InstVisitor<IntraBlockBuilder>,
                          BlockBuilderBase {
  friend class InstVisitor<IntraBlockBuilder>;

  // if true, cells are created for indirect callee
  bool m_track_callsites;

  void visitAllocaInst(AllocaInst &AI);
  void visitSelectInst(SelectInst &SI);
  void visitLoadInst(LoadInst &LI);
  void visitStoreInst(StoreInst &SI);
  void visitAtomicCmpXchgInst(AtomicCmpXchgInst &I);
  void visitAtomicRMWInst(AtomicRMWInst &I);
  void visitReturnInst(ReturnInst &RI);
  // void visitVAStart(CallSite CS);
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

  void visitCallBase(CallBase &I);
  void visitInlineAsmCall(CallBase &I);
  void visitExternalCall(CallBase &I);
  void visitIndirectCall(CallBase &I);
  void visitSeaDsaFnCall(CallBase &I);

  void visitAllocWrapperCall(CallBase &I);
  void visitAllocationFnCall(CallBase &I);


  SeadsaFn getSeaDsaFn(const Function *fn) {
    if (!fn) return SeadsaFn::UNKNOWN;

    SeadsaFn fnType = StringSwitch<SeadsaFn>(fn->getName())
                          .Case("sea_dsa_set_modified", SeadsaFn::MODIFY)
                          .Case("sea_dsa_set_read", SeadsaFn::READ)
                          .Case("sea_dsa_set_heap", SeadsaFn::HEAP)
                          .Case("sea_dsa_set_ptrtoint", SeadsaFn::PTR_TO_INT)
                          .Case("sea_dsa_set_inttoptr", SeadsaFn::INT_TO_PTR)
                          .Case("sea_dsa_set_extern", SeadsaFn::EXTERN)
                          .Case("sea_dsa_alias", SeadsaFn::ALIAS)
                          .Case("sea_dsa_collapse", SeadsaFn::COLLAPSE)
                          .Case("sea_dsa_mk_seq", SeadsaFn::MAKE_SEQ)
                          .Case("sea_dsa_mk", SeadsaFn::MAKE)
                          .Default(SeadsaFn::UNKNOWN);

    if (fnType != SeadsaFn::UNKNOWN) return fnType;

    // the spec generator requires sea_dsa_link to have type information, so we
    // may need to define multiple sea_dsa_link types. For example:
    // sea_dsa_link_to_charptr(const void *p, unsigned offset, const char*
    // p2);
    if (fn->getName().startswith("sea_dsa_link"))
      fnType = SeadsaFn::LINK;
    else if (fn->getName().startswith("sea_dsa_access"))
      fnType = SeadsaFn::ACCESS;

    return fnType;
  }

  /// Returns true if \p F is a \p seadsa_ family of functions
  static bool isSeaDsaFn(const Function *fn) {
    if (!fn) return false;
    auto n = fn->getName();
    return n.startswith("sea_dsa_");
  }

  // return function pointer to seadsa::Node setter
  using seadsaNodeSetterFunc =
      std::function<seadsa::Node &(seadsa::Node *, bool)>;
  seadsaNodeSetterFunc getSeaDsaAttrbFunc(const Function *fn) {
    switch (getSeaDsaFn(fn)) {
    case SeadsaFn::MODIFY:
      return &seadsa::Node::setModified;
    case SeadsaFn::READ:
      return &seadsa::Node::setRead;
    case SeadsaFn::HEAP:
      return &seadsa::Node::setHeap;
    case SeadsaFn::PTR_TO_INT:
      return &seadsa::Node::setPtrToInt;
    case SeadsaFn::INT_TO_PTR:
      return &seadsa::Node::setIntToPtr;
    case SeadsaFn::EXTERN:
      return &seadsa::Node::setExternal;
    default:
      return nullptr;
    }
  }

public:
  IntraBlockBuilder(Function &func, seadsa::Graph &graph, const DataLayout &dl,
                    const TargetLibraryInfo &tli,
                    const seadsa::AllocWrapInfo &allocInfo,
                    bool track_callsites)
      : BlockBuilderBase(func, graph, dl, tli, allocInfo),
        m_track_callsites(track_callsites) {}
};

seadsa::Cell BlockBuilderBase::valueCell(const Value &v) {
  using namespace seadsa;

  if (isNullConstant(v)) {
    LOG("dsa", errs() << "WARNING: constant not handled: " << v << "\n";);
    return Cell();
  }

  if (auto *ptrtoint = dyn_cast<PtrToIntInst>(&v)) {
    return valueCell(*ptrtoint->getOperand(0));
  }

  if (m_graph.hasCell(v)) {
    Cell &c = m_graph.mkCell(v, Cell());
    assert(!c.isNull());
    return c;
  }

  if (isa<UndefValue>(&v)) return Cell();
  if (isa<GlobalAlias>(&v))
    return valueCell(*cast<GlobalAlias>(&v)->getAliasee());

  if (isa<ConstantStruct>(&v) || isa<ConstantArray>(&v) ||
      isa<ConstantDataSequential>(&v) || isa<ConstantDataArray>(&v) ||
      isa<ConstantVector>(&v) || isa<ConstantDataVector>(&v)) {
    // XXX Handle properly once we have real examples with this failure
    LOG("dsa", errs() << "WARNING: unsound handling of an aggregate constant "
                         "as a fresh allocation: "
                      << v << "\n";);
    return m_graph.mkCell(v, Cell(m_graph.mkNode(), 0));
  }

  // -- special case for aggregate types. Cell creation is handled elsewhere
  if (v.getType()->isAggregateType()) return Cell();

  if (const ConstantExpr *ce = dyn_cast<const ConstantExpr>(&v)) {
    if (ce->isCast() && ce->getOperand(0)->getType()->isPointerTy())
      return valueCell(*ce->getOperand(0));
    else {
      Cell *C = nullptr;
      if (ce->getOpcode() == Instruction::GetElementPtr) {
        Value &base = *(ce->getOperand(0));

        // We create first a cell for the gep'd pointer operand if it's
        // an IntToPtr
        if (const ConstantExpr *base_ce = dyn_cast<ConstantExpr>(&base)) {
          if (base_ce->getOpcode() == Instruction::IntToPtr) {
            valueCell(base);
          }
        }
        SmallVector<Value *, 8> indicies(ce->op_begin() + 1, ce->op_end());
        visitGep(v, base, indicies);
        if (m_graph.hasCell(*ce)) { C = &(m_graph.mkCell(*ce, Cell())); }
      } else if (ce->getOpcode() == Instruction::IntToPtr) {
        visitCastIntToPtr(*ce);
        if (m_graph.hasCell(*ce)) { C = &(m_graph.mkCell(*ce, Cell())); }
      }

      if (C) {
        return *C;
      } else {
        LOG("dsa", errs() << "WARNING: unsound handling of a constant "
                          << "as a fresh allocation: " << *ce << "\n";);
        return m_graph.mkCell(v, Cell(m_graph.mkNode(), 0));
      }
    }
  }

  const auto &vType = *v.getType();
  assert(vType.isPointerTy() || vType.isAggregateType() || vType.isVectorTy());
  (void)vType;

  LOG("dsa-warn",
      errs() << "Unexpected expression at valueCell: " << v << "\n");
  assert(false && "Expression not handled");
  return Cell();
}

void IntraBlockBuilder::visitInstruction(Instruction &I) {
  if (isSkip(I)) return;

  m_graph.mkCell(I, seadsa::Cell(m_graph.mkNode(), 0));
}

void IntraBlockBuilder::visitAllocaInst(AllocaInst &AI) {
  using namespace seadsa;
  assert(!m_graph.hasCell(AI));
  Node &n = m_graph.mkNode();
  // -- record allocation site
  seadsa::DsaAllocSite *site = m_graph.mkAllocSite(AI);
  assert(site);
  n.addAllocSite(*site);
  // -- mark node as a stack node
  n.setAlloca();

  m_graph.mkCell(AI, Cell(n, 0));
}

void IntraBlockBuilder::visitSelectInst(SelectInst &SI) {
  using namespace seadsa;
  if (isSkip(SI)) return;

  assert(!m_graph.hasCell(SI));

  Cell thenC = valueCell(*SI.getOperand(1));
  Cell elseC = valueCell(*SI.getOperand(2));
  thenC.unify(elseC);

  // -- create result cell
  m_graph.mkCell(SI, Cell(thenC, 0));
}

void IntraBlockBuilder::visitLoadInst(LoadInst &LI) {
  using namespace seadsa;

  // -- skip read from NULL
  if (BlockBuilderBase::isNullConstant(*LI.getPointerOperand())) return;

  if (!m_graph.hasCell(*LI.getPointerOperand()->stripPointerCasts())) {
    /// XXX: this is very likely because the pointer operand is the
    /// result of applying one or several gep instructions starting
    /// from NULL. Note that this is undefined behavior but it
    /// occurs in ldv benchmarks.
    if (!isa<ConstantExpr>(LI.getPointerOperand()->stripPointerCasts())) return;
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
            seadsa::DsaAllocSite *site = m_graph.mkAllocSite(*F);
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
      base.addLink(LoadedField, Cell(&n, 0));
    }

    auto &c = m_graph.mkCell(LI, base.getLink(LoadedField));
    (void)c;
    assert(!c.isNull());
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
  using namespace seadsa;

  if (!m_graph.hasCell(*I.getPointerOperand()->stripPointerCasts())) { return; }
  Value *Ptr = I.getPointerOperand();
  Value *New = I.getNewValOperand();

  Cell PtrC = valueCell(*Ptr);
  assert(!PtrC.isNull());

  assert(isa<StructType>(I.getType()));
  Type *ResTy = cast<StructType>(I.getType())->getTypeAtIndex((unsigned)0);

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
      PtrC.addLink(LoadedField, Cell(n, 0));
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

  seadsa::Cell PtrC = valueCell(*Ptr);
  assert(!PtrC.isNull());

  PtrC.setModified();
  PtrC.setRead();
  PtrC.growSize(0, I.getType());
  PtrC.addAccessedType(0, I.getType());

  if (!isSkip(I)) {
    seadsa::Node &n = m_graph.mkNode();
    seadsa::Cell ResC = m_graph.mkCell(I, seadsa::Cell(n, 0));
    ResC.unify(PtrC);
  }
}

static bool isBytePtrTy(const Type *ty) {
  return ty->isPointerTy() && ty->getPointerElementType()->isIntegerTy(8);
}

void IntraBlockBuilder::visitStoreInst(StoreInst &SI) {
  using namespace seadsa;

  // -- skip store into NULL
  if (BlockBuilderBase::isNullConstant(
          *SI.getPointerOperand()->stripPointerCasts()))
    return;

  if (!m_graph.hasCell(*SI.getPointerOperand()->stripPointerCasts())) {
    /// XXX: this is very likely because the pointer operand is the
    /// result of applying one or several gep instructions starting
    /// from NULL. Note that this is undefined behavior but it
    /// occurs in ldv benchmarks.
    if (!isa<ConstantExpr>(SI.getPointerOperand()->stripPointerCasts())) return;
  }

  Cell base = valueCell(*SI.getPointerOperand()->stripPointerCasts());
  assert(!base.isNull());

  base.setModified();

  Value *ValOp = SI.getValueOperand();
  // XXX: potentially it is enough to update size only at this point
  base.growSize(0, ValOp->getType());
  base.addAccessedType(0, ValOp->getType());

  if (isSkip(*ValOp)) return;

  if (BlockBuilderBase::isNullConstant(*ValOp)) {
    // TODO: mark link as possibly pointing to null
    return;
  }
  if (isa<UndefValue>(ValOp)) {
    LOG("dsa-warn",
        errs() << "WARNING: Storing undef value into a pointer field: " << SI
               << "\n";);
    return;
  }

  Cell val = valueCell(*ValOp);
  assert(!val.isNull());

  // val.getType() can be an opaque type, so we cannot use it to get
  // a ptr type.
  Cell dest(val.getNode(), val.getRawOffset());

  // -- guess best type for the store. Use the type of the value being
  // -- stored, unless it is i8*, in which case check if the store location
  // -- has a better type
  Type *ty = ValOp->getType();
  if (isBytePtrTy(ty)) {
    Type *opTy = SI.getPointerOperand()->stripPointerCasts()->getType();
    if (opTy->isPointerTy() && opTy->getPointerElementType()->isPointerTy())
      ty = opTy->getPointerElementType();
  }
  base.addLink(Field(0, FieldType(ty)), dest);
}

void IntraBlockBuilder::visitBitCastInst(BitCastInst &I) {
  if (isSkip(I)) return;

  if (BlockBuilderBase::isNullConstant(*I.getOperand(0)))
    return; // do nothing if null

  seadsa::Cell arg = valueCell(*I.getOperand(0));
  assert(!arg.isNull());
  m_graph.mkCell(I, arg);
}

// greatest common divisor
template <typename T> T gcd(T a, T b) {
  T c;
  while (b) {
    c = a % b;
    a = b;
    b = c;
  }
  return a;
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
      else if (Ty->isArrayTy())
        Ty = Ty->getArrayElementType();
      else if (auto vt = dyn_cast<VectorType>(Ty))
        Ty = vt->getElementType();
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
      else if (ty->isArrayTy())
        ty = ty->getArrayElementType();
      else if (auto vt = dyn_cast<VectorType>(ty))
        ty = vt->getElementType();
      assert(ty && "Type is neither PointerType nor SequentialType");
      offset += idx * dl.getTypeAllocSize(ty);
    }
  }
  return offset;
}

void BlockBuilderBase::visitGep(const Value &gep, const Value &ptr,
                                ArrayRef<Value *> indicies) {
  // -- skip NULL
  if (isNullConstant(ptr)) return;

  //  -- skip load from null
  if (auto *LI = dyn_cast<LoadInst>(&ptr)) {
    if (BlockBuilderBase::isNullConstant(*(LI->getPointerOperand()))) return;
  }

  // -- skip gep from null
  if (auto *GEP = dyn_cast<GetElementPtrInst>(&ptr)) {
    if (BlockBuilderBase::isNullConstant(*(GEP->getPointerOperand()))) return;
  }

  // -- empty gep that points directly to the base
  if (gep.stripPointerCasts() == &ptr) return;

  if (m_graph.hasCell(gep)) {
    // gep can have already a cell if it can be stripped to another
    // pointer different from the base or is a constant gep that has already
    // been visited.
    assert(gep.stripPointerCasts() != &gep || llvm::isa<ConstantExpr>(&gep));
    return;
  }

  seadsa::Cell base = valueCell(ptr);

  if (base.isNull()) {
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

  assert(!m_graph.hasCell(gep));
  assert(!base.isNull());
  seadsa::Node *baseNode = base.getNode();
  if (baseNode->isOffsetCollapsed()) {
    m_graph.mkCell(gep, seadsa::Cell(baseNode, 0));
    return;
  }

  auto off = computeGepOffset(ptr.getType(), indicies, m_dl);
  if (off.first < 0) {
    if (base.getOffset() + off.first >= 0) {
      m_graph.mkCell(gep,
                     seadsa::Cell(*baseNode, base.getOffset() + off.first));
      return;
    } else {
      // create a new node for gep
      seadsa::Node &n = m_graph.mkNode();
      // gep points to offset 0
      m_graph.mkCell(gep, seadsa::Cell(n, 0));
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
    seadsa::Node &n = m_graph.mkNode();
    n.setArraySize(off.second);
    // result of the gep points into that array at the gep offset
    // plus the offset of the base
    m_graph.mkCell(gep, seadsa::Cell(n, off.first + base.getRawOffset()));
    // finally, unify array with the node of the base
    n.unify(*baseNode);
  } else {
    m_graph.mkCell(gep, seadsa::Cell(base, off.first));
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
    LOG("dsa-warn", errs() << "WARNING: Gep inst with a shuffle vector operand "
                           << "is allocating a new cell: " << I << "\n");
    m_graph.mkCell(I, seadsa::Cell(m_graph.mkNode(), 0));
    return;
  }

  SmallVector<Value *, 8> indicies(I.op_begin() + 1, I.op_end());
  visitGep(I, ptr, indicies);
}

void IntraBlockBuilder::visitInsertValueInst(InsertValueInst &I) {
  assert(I.getAggregateOperand()->getType() == I.getType());
  using namespace seadsa;

  // make sure that the aggregate has a cell
  Cell op = valueCell(*I.getAggregateOperand()->stripPointerCasts());
  if (op.isNull()) {
    Node &n = m_graph.mkNode();
    // -- record allocation site
    seadsa::DsaAllocSite *site = m_graph.mkAllocSite(I);
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
  if (!isSkip(v) && !isNullConstant(v) && !isa<UndefValue>(v)) {
    // TODO: follow valueCell ptrs.
    Cell vCell = valueCell(v);
    assert(!vCell.isNull());
    out.addLink(Field(0, FieldType(v.getType())), vCell);
  }
}

void IntraBlockBuilder::visitExtractValueInst(ExtractValueInst &I) {
  using namespace seadsa;
  Cell op = valueCell(*I.getAggregateOperand()->stripPointerCasts());
  if (op.isNull()) {
    Node &n = m_graph.mkNode();
    // -- record allocation site
    seadsa::DsaAllocSite *site = m_graph.mkAllocSite(I);
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
      in.addLink(InstType, Cell(&n, 0));
      // -- record allocation site
      seadsa::DsaAllocSite *site = m_graph.mkAllocSite(I);
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

void IntraBlockBuilder::visitInlineAsmCall(CallBase &I) {
  using namespace seadsa;
  assert(I.isInlineAsm());

  if (isSkip(I)) return;
  Cell &c = m_graph.mkCell(I, Cell(m_graph.mkNode(), 0));
  c.getNode()->setExternal();

  // treat as allocation site
  seadsa::DsaAllocSite *site = m_graph.mkAllocSite(I);
  assert(site);
  c.getNode()->addAllocSite(*site);
}

void IntraBlockBuilder::visitExternalCall(CallBase &I) {
  using namespace seadsa;
  if (isSkip(I)) return;

  auto *callee = getCalledFunction(I);
  assert(callee);
  assert(callee->isDeclaration());

  Cell &c = m_graph.mkCell(I, Cell(m_graph.mkNode(), 0));
  c.getNode()->setExternal();

  if (callee->getName().startswith("verifier.nondet.abstract.memory")) return;

  // TODO: better handling of external funcations
  // TOOD: Use function attributes and external specifications

  LOG("dsa", errs() << "Visiting call to an external function:\n"
                    << "func: " << *callee << "\n"
                    << "inst: " << I << "\n";);

  if (auto *V = llvm::getArgumentAliasingToReturnedPointer(cast<CallBase>(&I),
                                                           false)) {
    // arg V is returned by the call, unify it with return
    assert(m_graph.hasCell(*V));
    Cell &argC = m_graph.mkCell(*V, Cell());
    c.unify(argC);
  } else if (callee->returnDoesNotAlias()) {
    // if return does not alias, it is an allocation site hidden inside the
    // function
    seadsa::DsaAllocSite *site = m_graph.mkAllocSite(I);
    assert(site);
    c.getNode()->addAllocSite(*site);
  } else if (AssumeExternalFunctonsAllocators) {
    // -- assume that every external function returns a freshly allocated
    // pointer
    seadsa::DsaAllocSite *site = m_graph.mkAllocSite(I);
    assert(site);
    c.getNode()->addAllocSite(*site);
  }
}

void IntraBlockBuilder::visitIndirectCall(CallBase &I) {
  using namespace seadsa;
  if (!isSkip(I)) m_graph.mkCell(I, Cell(m_graph.mkNode(), 0));

  if (!m_track_callsites) return;

  assert(I.getCalledOperand());
  const Value &calledV = *(I.getCalledOperand());
  if (m_graph.hasCell(calledV)) {
    Cell calledC = m_graph.getCell(calledV);
    m_graph.mkCallSite(I, calledC);
  } else {
    LOG("dsa",
        errs() << "WARNING: no cell found for callee in indirect call.\n";);
  }
}

void IntraBlockBuilder::visitSeaDsaFnCall(CallBase &I) {
  using namespace llvm::PatternMatch;
  using namespace seadsa;
  auto *callee = getCalledFunction(I);
  SeadsaFn fn = getSeaDsaFn(callee);

  switch (fn) {
  // attribute setters all have same behaviour
  case SeadsaFn::MODIFY:
  case SeadsaFn::READ:
  case SeadsaFn::HEAP:
  case SeadsaFn::EXTERN:
  case SeadsaFn::INT_TO_PTR:
  case SeadsaFn::PTR_TO_INT: {
    // sea_dsa_read(const void *p) -- mark the node pointed to by p as read
    Value *arg = nullptr;
    if (!(match(I.getArgOperand(0), m_Value(arg)) &&
          arg->getType()->isPointerTy()))
      return;
    if (isSkip(*(arg))) return;
    if (m_graph.hasCell(*(arg))) {
      seadsa::Cell c = valueCell(*(arg));

      // get the appropriate setter, and execute on the object instance
      seadsaNodeSetterFunc setter = getSeaDsaAttrbFunc(callee);
      if (setter) setter(c.getNode(), true);
    }
    return;
  }

  case SeadsaFn::ALIAS: {
    llvm::SmallVector<seadsa::Cell, 8> toMerge;
    LOG("sea-dsa-alias", llvm::errs() << "In alias\n";);
    unsigned nargs = I.arg_size();
    for (unsigned i = 0; i < nargs; ++i) {
      Value *arg = nullptr;
      if (!(match(I.getArgOperand(i), m_Value(arg)) &&
            arg->getType()->isPointerTy()))
        continue;
      if (isSkip(*arg)) {
        LOG("sea-dsa-alias", llvm::errs()
                                 << "Skip arg: " << *arg << " reason(1)\n";);
        continue;
      }
      arg = arg->stripPointerCasts();
      auto &ArgRef = *arg;
      if (!m_graph.hasCell(ArgRef)) {
        LOG("sea-dsa-alias", llvm::errs()
                                 << "Skip arg: " << *arg << " reason(2)\n";);
        continue;
      }
      seadsa::Cell c = valueCell(ArgRef);
      if (c.isNull()) {
        LOG("sea-dsa-alias", llvm::errs()
                                 << "Skip arg: " << *arg << " reason(2)\n";);
        continue;
      }
      LOG("sea-dsa-alias", llvm::errs() << "Add to merge: " << ArgRef << "\n";);
      toMerge.push_back(c);
    }
    LOG("sea-dsa-alias", {
      llvm::errs() << "Merging: ";
      for (auto &v : toMerge) {
        llvm::errs() << "\t" << v << "\n";
      }
    });

    for (unsigned i = 1; i < toMerge.size(); ++i)
      toMerge[0].unify(toMerge[i]);
    return;
  }

  case SeadsaFn::COLLAPSE: {
    // seadsa_collapse(p) -- collapse the node to which p points to
    Value *arg = nullptr;
    if (!(match(I.getArgOperand(0), m_Value(arg)) &&
          arg->getType()->isPointerTy()))
      return;
    if (isSkip(*(arg))) return;
    if (m_graph.hasCell(*(arg))) {
      seadsa::Cell c = valueCell(*(arg));
      c.getNode()->collapseOffsets(__LINE__);
    }
    return;
  }

  case SeadsaFn::MAKE_SEQ: {
    // seadsa_mk_seq(p, sz) -- mark the node pointed by p as sequence of size sz
    Value *arg0 = nullptr;
    ConstantInt *arg1 = nullptr;

    if (!(match(I.getArgOperand(0), m_Value(arg0)) &&
          arg0->getType()->isPointerTy()))
      return;
    if (!match(I.getArgOperand(1), m_ConstantInt(arg1))) return;
    if (isSkip(*arg0)) return;
    if (!m_graph.hasCell(*arg0)) return;

    seadsa::Cell c = valueCell(*arg0);
    Node *n = c.getNode();
    if (!n->isArray()) {

      assert(!arg1->isZero() && "Second argument must be a number!");
      if (arg1->isZero()) return;

      if (n->size() <= arg1->getZExtValue()) {
        n->setArraySize(arg1->getZExtValue());
      } else {
        LOG("dsa",
            errs() << "WARNING: skipped " << I << " because new size cannot be"
                   << " smaller than the size of the node pointed by the "
                      "pointer.\n";);
      }
    } else {
      LOG("dsa", errs() << "WARNING: skipped " << I
                        << " because it expects a pointer"
                        << " that points to a non-sequence node.\n";);
    }
    return;
  }

  case SeadsaFn::LINK: {
    // seadsa_load(p, off) -- return pointer at offset
    Value *arg0 = nullptr;
    ConstantInt *arg1 = nullptr;
    Value *arg2 = nullptr;

    if (!(match(I.getArgOperand(0), m_Value(arg0)) &&
          arg0->getType()->isPointerTy()))
      return;
    if (!match(I.getArgOperand(1), m_ConstantInt(arg1))) return;
    if (!(match(I.getArgOperand(2), m_Value(arg2)) &&
          arg2->getType()->isPointerTy()))
      return;

    if (isSkip(*arg0) || isSkip(*arg2)) return;
    if (!m_graph.hasCell(*arg0) || !m_graph.hasCell(*arg2)) return;

    seadsa::Node *n = m_graph.getCell(*arg0).getNode();
    seadsa::Cell c2 = m_graph.getCell(*arg2);
    n->addLink(Field(arg1->getZExtValue(), FieldType(arg2->getType())), c2);

    return;
  }

  case SeadsaFn::MAKE: {
    // seadsa_set_type(str_type) -- set the node type to the given type
    Node &n = m_graph.mkNode();
    m_graph.mkCell(I, Cell(n, 0));
    return;
  }

  case SeadsaFn::ACCESS: {
    // seadsa_access_<type>(<type> *p, unsigned o) -- node accesses <type> at o
    Value *arg0 = nullptr;
    ConstantInt *arg1 = nullptr;

    if (!(match(I.getArgOperand(0), m_Value(arg0)) &&
          arg0->getType()->isPointerTy()))
      return;
    if (!match(I.getArgOperand(1), m_ConstantInt(arg1))) return;
    if (isSkip(*arg0)) return;
    if (!m_graph.hasCell(*arg0)) return;

    seadsa::Node *n = m_graph.getCell(*arg0).getNode();
    n->addAccessedType(arg1->getZExtValue(), arg0->getType());
    return;
  }

  default: {
    visitExternalCall(I);
    return;
  }
  }
}

void IntraBlockBuilder::visitAllocWrapperCall(CallBase &I) {
  visitAllocationFnCall(I);
}

void IntraBlockBuilder::visitAllocationFnCall(CallBase &I) {
  using namespace seadsa;
  Node &n = m_graph.mkNode();
  // -- record allocation site
  seadsa::DsaAllocSite *site = m_graph.mkAllocSite(I);
  assert(site);
  n.addAllocSite(*site);
  // -- mark node as a heap node
  n.setHeap();

  m_graph.mkCell(I, Cell(n, 0));
}

void IntraBlockBuilder::visitCallBase(CallBase &I) {
  using namespace seadsa;

  if (I.isInlineAsm()) {
    visitInlineAsmCall(I);
    return;
  }

  if (I.isIndirectCall()) {
    visitIndirectCall(I);
    return;
  }

  if (llvm::isAllocationFn(&I, &m_tli)) {
    visitAllocationFnCall(I);
    return;
  }

  // direct function call
  if (auto *callee = getCalledFunction(I)) {
    if (m_allocInfo.isAllocWrapper(*callee)) {
      visitAllocWrapperCall(I);
      return;
    } else if (isSeaDsaFn(callee)) {
      visitSeaDsaFnCall(I);
      return;
    } else if (callee->isDeclaration()) {
      visitExternalCall(I);
      return;
    }
  }

  // not handled direct call
  if (!isSkip(I)) {
    m_graph.mkCell(I, Cell(m_graph.mkNode(), 0));
    return;
  }

  // a function that does not return a pointer is a noop
  if (!I.getType()->isPointerTy()) return;


  // -- something unexpected. Keep an assert so that we know that something
  // -- unexpected happened.

  // calls that have no pointer arg
  // nothing is expected here
  assert(false && "Unexpected CallSite");
  llvm_unreachable("Unexpected");
  // TODO: handle variable argument functions
}

void IntraBlockBuilder::visitShuffleVectorInst(ShuffleVectorInst &I) {
  using namespace seadsa;

  // XXX: TODO: handle properly.
  LOG("dsa-warn",
      errs() << "WARNING: shuffle vector inst is allocating a new cell: " << &I
             << "\n");
  m_graph.mkCell(I, Cell(m_graph.mkNode(), 0));
}

void IntraBlockBuilder::visitMemSetInst(MemSetInst &I) {
  seadsa::Cell dest = valueCell(*(I.getDest()));
  // assert (!dest.isNull ());
  if (!dest.isNull()) dest.setModified();

  // TODO:
  // can also update size using I.getLength ()
}

bool hasNoPointerTy(const llvm::Type *t) {
  if (!t) return true;

  if (t->isPointerTy()) return false;
  if (const StructType *sty = dyn_cast<const StructType>(t)) {
    for (auto it = sty->element_begin(), end = sty->element_end(); it != end;
         ++it)
      if (!hasNoPointerTy(*it)) return false;
  } else if (t->isArrayTy())
    return hasNoPointerTy(t->getArrayElementType());
  else if (auto vt = dyn_cast<const VectorType>(t))
    return hasNoPointerTy(vt->getElementType());

  return true;
}

bool transfersNoPointers(MemTransferInst &MI, const DataLayout &DL) {
  Value *srcPtr = MI.getSource();
  auto *srcTy = srcPtr->getType()->getPointerElementType();

  ConstantInt *rawLength = dyn_cast<ConstantInt>(MI.getLength());
  if (!rawLength) return false;

  const uint64_t length = rawLength->getZExtValue();
  LOG("dsa", errs() << "MemTransfer length:\t" << length << "\n");

  // opaque structs may transfer pointers
  if (!srcTy->isSized()) return false;

  // TODO: Go up to the GEP chain to find nearest fitting type to transfer.
  // This can occur when someone tries to transfer int the middle of a struct.
  if (length * 8 > DL.getTypeSizeInBits(srcTy)) {
    LOG("dsa-warn", errs() << "WARNING: MemTransfer past object size!\n"
                           << "\tTransfer:  ");
    LOG("dsa", MI.print(errs()));
    LOG("dsa-warn", errs() << "\n\tLength:  " << length << "\n\tType size:  "
                           << (DL.getTypeSizeInBits(srcTy) / 8) << "\n");
    return false;
  }

  static SmallDenseMap<std::pair<Type *, unsigned>, bool, 16>
      knownNoPointersInStructs;

  if (knownNoPointersInStructs.count({srcTy, length}) != 0)
    return knownNoPointersInStructs[{srcTy, length}];

  for (auto &subTy : seadsa::AggregateIterator::range(srcTy, &DL)) {
    if (subTy.Offset >= length) break;

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
  if (isNullConstant(*I.getSource())) return;

  // -- skip copy to an unallocated address
  if (isNullConstant(*I.getDest())) return;

  if (!m_graph.hasCell(*I.getSource())) {
    LOG("dsa-warn", errs() << "WARNING: source of memcopy/memmove has no cell: "
                           << *I.getSource() << "\n");
    return;
  }

  if (!m_graph.hasCell(*I.getDest())) {
    LOG("dsa-warn",
        errs() << "WARNING: destination of memcopy/memmove has no cell: "
               << *I.getDest() << "\n");
    return;
  }

  // -- trust types, i.e., assume types are not abused by storing
  // -- pointers pointers to other types

  bool TrustTypes = true;
  assert(m_graph.hasCell(*I.getDest()));
  assert(m_graph.hasCell(*I.getSource()));

  // unify the two cells because potentially all bytes of source
  // are copied into dest
  seadsa::Cell sourceCell = valueCell(*I.getSource());
  seadsa::Cell destCell = m_graph.mkCell(*I.getDest(), seadsa::Cell());

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

  if (def.hasNUses(0)) return false;

  if (def.hasOneUse()) {
    const Value *v = dyn_cast<Value>(def.use_begin()->getUser());
    if (isa<CmpInst>(v)) return false;

    DenseSet<const Value *> seen;
    while (v && v->hasOneUse() && seen.insert(v).second) {
      if (isa<LoadInst>(v) || isa<StoreInst>(v) || isa<CallInst>(v)) break;
      v = dyn_cast<const Value>(v->use_begin()->getUser());
    }
    if (isa<CmpInst>(v)) return false;
  }
  return true;
}

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
      seadsa::Cell ptrCell =
          valueCell(*LI->getPointerOperand()->stripPointerCasts());
      ptrCell.addAccessedType(0, liType);
      ptrCell.setRead();
      seadsa::Field LoadedField(0, seadsa::FieldType(liType));
      if (!ptrCell.hasLink(LoadedField)) {
        seadsa::Node &n = m_graph.mkNode();
        // XXX: This node will have no allocation site
        ptrCell.addLink(LoadedField, seadsa::Cell(&n, 0));
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
        seadsa::Cell baseCell = valueCell(*base);
        assert(!baseCell.isNull());

        assert(!m_graph.hasCell(dest) && "Untested");
        seadsa::Node *baseNode = baseCell.getNode();
        if (baseNode->isOffsetCollapsed())
          m_graph.mkCell(dest, seadsa::Cell(baseNode, 0));
        else
          m_graph.mkCell(dest, seadsa::Cell(baseCell, offset));
        return;
      }
    }
  }

  seadsa::Node &n = m_graph.mkNode();
  n.setIntToPtr();
  // -- record allocation site
  seadsa::DsaAllocSite *site = m_graph.mkAllocSite(dest);
  assert(site);
  n.addAllocSite(*site);
  // -- mark node as an alloca node
  n.setAlloca();
  m_graph.mkCell(dest, seadsa::Cell(n, 0));
  if (shouldBeTrackedIntToPtr(dest) && !m_graph.isFlat())
    LOG("dsa-warn",
        llvm::errs()
            << "WARNING: " << dest << " @ addr " << &dest
            << " is (unsoundly) assumed to point to a fresh memory region.\n";);
}

void IntraBlockBuilder::visitIntToPtrInst(IntToPtrInst &I) {
  visitCastIntToPtr(I);
}

void IntraBlockBuilder::visitReturnInst(ReturnInst &RI) {
  Value *v = RI.getReturnValue();

  // We don't skip the return value if its type contains a pointer.
  if (!v || (isSkip(*v) && !containsPointer(*v))) { return; }

  seadsa::Cell c = valueCell(*v);
  if (c.isNull()) return;

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
    if (!seen.insert(v).second) continue;

    for (auto *user : v->users()) {
      if (seen.count(user)) continue;
      if (isa<BranchInst>(user) || isa<CmpInst>(user)) continue;
      if (auto *si = dyn_cast<SelectInst>(user)) {
        if (si->getCondition() == v) continue;
      }
      if (auto *bi = dyn_cast<BinaryOperator>(user)) {
        if (bi->getOpcode() == Instruction::Sub &&
            isa<PtrToIntInst>(bi->getOperand(0)) &&
            isa<PtrToIntInst>(bi->getOperand(1)))
          continue;
      }
      if (auto *CI = dyn_cast<const CallInst>(user)) {
        const Function *callee = CI->getCalledFunction();
        if (callee) {
          // callee might not access memory but it can return ptrtoint passed to
          // it if (callee->doesNotAccessMemory())
          //   continue;
          auto n = callee->getName();
          if (n.startswith("__sea_set_extptr_slot") ||
              n.equals("verifier.assume") || n.equals("llvm.assume"))
            continue;
        }
      }

      // if the value flows into one of these operands, we consider it
      // escaping and stop tracking it further
      if (isa<LoadInst>(*user) || isa<StoreInst>(*user) ||
          isa<CallInst>(*user) || isa<IntToPtrInst>(*user))
        return true;

      workList.push_back(user);
    }
  }
  return false;
}

void IntraBlockBuilder::visitPtrToIntInst(PtrToIntInst &I) {
  if (!isEscapingPtrToInt(I)) return;

  if (BlockBuilderBase::isNullConstant(*I.getOperand(0))) return;

  // -- conversion of a pointer that has no memory representation
  // -- something is wrong, do a warning and return
  if (!m_graph.hasCell(*I.getOperand(0))) {
    LOG("dsa", llvm::errs()
                   << "Warning: ptrtoint applied to pointer without a cell\n";);
    return;
  }
  assert(m_graph.hasCell(*I.getOperand(0)));
  seadsa::Cell c = valueCell(*I.getOperand(0));
  if (!c.isNull()) {

    // mark node as having a pointer that escapes
    c.getNode()->setPtrToInt(true);

    // print a warning, more verbose under dsa LOG
    LOG("dsa", llvm::errs()
                   << "WARNING: " << I
                   << " might be escaping as is not tracked further\n");
    LOG("dsa-warn",
        llvm::errs() << "WARNING: detected ptrtoint instruction that might be "
                        "escaping the analysis. "
                     << "(@" << intptr_t(&I) << ")\n");
  }
}

} // end namespace

/*****************************************************************************/
/* PUBLIC API                                                                */
/*****************************************************************************/
namespace seadsa {

/*****************************************************************************/
/* LocalAnalysis                                                             */
/*****************************************************************************/

void LocalAnalysis::runOnFunction(Function &F, Graph &g) {
  LOG("dsa-progress",
      errs() << "Running seadsa::Local on " << F.getName() << "\n");

  auto &tli = m_tliWrapper.getTLI(F);
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
        seadsa::DsaAllocSite *site = g.mkAllocSite(a);
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
    GlobalBuilder globalBuilder(F, g, m_dl, tli, m_allocInfo);
    globalBuilder.initGlobalVariables();
  }

  IntraBlockBuilder intraBuilder(F, g, m_dl, tli, m_allocInfo,
                                 m_track_callsites);
  InterBlockBuilder interBuilder(F, g, m_dl, tli, m_allocInfo);
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
            errs() << "SCALAR " << *(kv.first) << "\n"
                   << "WARNING: a node has no allocation site\n";
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

/*****************************************************************************/
/* Local -- LLVM Pass to run LocalAnalysis                                   */
/*****************************************************************************/

Local::Local() : ModulePass(ID), m_dl(nullptr), m_allocInfo(nullptr) {}

void Local::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<TargetLibraryInfoWrapperPass>();
  // dependency for immutable AllowWrapInfo
  AU.addRequired<LoopInfoWrapperPass>();
  AU.addRequired<AllocWrapInfo>();
  AU.setPreservesAll();
}

bool Local::runOnModule(Module &M) {
  m_dl = &M.getDataLayout();
  m_allocInfo = &getAnalysis<AllocWrapInfo>();
  m_allocInfo->initialize(M, this);

  for (Function &F : M)
    runOnFunction(F);
  return false;
}

bool Local::runOnFunction(Function &F) {
  if (F.isDeclaration() || F.empty()) return false;

  LOG("progress", errs() << "DSA: " << F.getName() << "\n";);

  auto &tli = getAnalysis<TargetLibraryInfoWrapperPass>();
  LocalAnalysis la(*m_dl, tli, *m_allocInfo);
  GraphRef g = std::make_shared<Graph>(*m_dl, m_setFactory);
  la.runOnFunction(F, *g);
  m_graphs.insert({&F, g});
  return false;
}

bool Local::hasGraph(const Function &F) const { return m_graphs.count(&F); }

const Graph &Local::getGraph(const Function &F) const {
  auto it = m_graphs.find(&F);
  assert(it != m_graphs.end());
  return *(it->second);
}

} // namespace seadsa

char seadsa::Local::ID = 0;

static llvm::RegisterPass<seadsa::Local>
    X("seadsa-local", "Flow-insensitive, intra-procedural SeaDsa analysis");
