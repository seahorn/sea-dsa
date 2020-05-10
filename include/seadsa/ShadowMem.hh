#pragma once
/****
 * Instrument the bitcode with shadow instructions used to construct
 * Memory SSA form.
 ****/

#include "llvm/ADT/STLExtras.h"
#include "llvm/Analysis/MemorySSA.h"
#include "llvm/Pass.h"
#include <memory>

#include "seadsa/DsaAnalysis.hh"

namespace llvm {
class Value;
class Function;
class TargetLibraryInfo;
class TargetLibraryInfoWrapperPass;
class DataLayout;
class CallGraph;
class CallInst;
class ImmutableCallSite;
} // namespace llvm

namespace seadsa {
class ShadowMemImpl;
class GlobalAnalysis;
class AllocSiteInfo;
class Cell;
} // namespace seadsa

namespace seadsa {
using namespace llvm;

namespace SMSSAHelpers {
struct AllAccessTag {};
} // end namespace SMSSAHelpers

// The base for all memory accesses. All memory accesses in a block are
// linked together using an intrusive list.
class SeaMemoryAccess
    : public DerivedUser,
      public ilist_node<SeaMemoryAccess,
                        ilist_tag<SMSSAHelpers::AllAccessTag>> {
public:
  using AllAccessType =
      ilist_node<SeaMemoryAccess, ilist_tag<SMSSAHelpers::AllAccessTag>>;

  SeaMemoryAccess(const SeaMemoryAccess &) = delete;
  SeaMemoryAccess &operator=(const SeaMemoryAccess &) = delete;

  void *operator new(size_t) = delete;

  // Methods for support type inquiry through isa, cast, and
  // dyn_cast
  static bool classof(const Value *V) {
    unsigned ID = V->getValueID();
    // XXX Reuse IDs from MemorySSA. Be very careful mixing llvm::MemorySSA
    // classes with these ones
    return ID == MemoryUseVal || ID == MemoryPhiVal || ID == MemoryDefVal;
  }

  BasicBlock *getBlock() const { return Block; }

  void print(raw_ostream &OS) const;
  void dump() const;

  /// The user iterators for a memory access
  using iterator = user_iterator;
  using const_iterator = const_user_iterator;

  /// We default to the all access list.
  AllAccessType::self_iterator getIterator() {
    return this->AllAccessType::getIterator();
  }
  AllAccessType::const_self_iterator getIterator() const {
    return this->AllAccessType::getIterator();
  }
  AllAccessType::reverse_self_iterator getReverseIterator() {
    return this->AllAccessType::getReverseIterator();
  }
  AllAccessType::const_reverse_self_iterator getReverseIterator() const {
    return this->AllAccessType::getReverseIterator();
  }

  void setDsaCell(const seadsa::Cell &c) { m_Cell = c; }
  void setDsaCell(const llvm::Optional<Cell> &c) {
    if (c.hasValue()) m_Cell = c.getValue();
  }
  const seadsa::Cell &getDsaCell() const { return m_Cell; }

protected:
  friend class SeaMemoryDef;
  friend class SeaMemoryPhi;
  friend class SeaMemorySSA;
  friend class SeaMemoryUse;
  friend class SeaMemoryUseOrDef;

  /// Used by MemorySSA to change the block of a MemoryAccess when it is
  /// moved.
  void setBlock(BasicBlock *BB) { Block = BB; }

  /// Used for debugging and tracking things about MemoryAccesses.
  /// Guaranteed unique among MemoryAccesses, no guarantees otherwise.
  inline unsigned getID() const;

  SeaMemoryAccess(LLVMContext &C, unsigned Vty, DeleteValueTy DeleteValue,
                  BasicBlock *BB, unsigned NumOperands)
      : DerivedUser(Type::getVoidTy(C), Vty, nullptr, NumOperands, DeleteValue),
        Block(BB) {}

  // Use deleteValue() to delete a generic MemoryAccess.
  ~SeaMemoryAccess() = default;

private:
  BasicBlock *Block;
  seadsa::Cell m_Cell;
};

inline llvm::raw_ostream &operator<<(llvm::raw_ostream &OS,
                                     const seadsa::SeaMemoryAccess &MA) {
  MA.print(OS);
  return OS;
}

} // namespace seadsa

namespace llvm {
template <> struct ilist_alloc_traits<seadsa::SeaMemoryAccess> {
  static void deleteNode(seadsa::SeaMemoryAccess *MA) { MA->deleteValue(); }
};

} // namespace llvm

namespace seadsa {

class SeaMemoryUseOrDef : public SeaMemoryAccess {
public:
  void *operator new(size_t) = delete;

  DECLARE_TRANSPARENT_OPERAND_ACCESSORS(SeaMemoryAccess);

  /// Get the instruction that this MemoryUse represents.
  Instruction *getMemoryInst() const { return MemoryInstruction; }

  /// Get the access that produces the memory state used by this Use.
  SeaMemoryAccess *getDefiningAccess() const { return getOperand(0); }

  static bool classof(const Value *MA) {
    return MA->getValueID() == MemoryUseVal || MA->getValueID() == MemoryDefVal;
  }

  // Sadly, these have to be public because they are needed in some of the
  // iterators.
  inline bool isOptimized() const;
  inline SeaMemoryAccess *getOptimized() const;
  inline void setOptimized(SeaMemoryAccess *);

  // Retrieve AliasResult type of the optimized access. Ideally this would be
  // returned by the caching walker and may go away in the future.
  Optional<AliasResult> getOptimizedAccessType() const {
    return OptimizedAccessAlias;
  }

  /// Reset the ID of what this MemoryUse was optimized to, causing it to
  /// be rewalked by the walker if necessary.
  /// This really should only be called by tests.
  inline void resetOptimized();

protected:
  friend class SeaMemorySSA;

  SeaMemoryUseOrDef(LLVMContext &C, SeaMemoryAccess *DMA, unsigned Vty,
                    DeleteValueTy DeleteValue, Instruction *MI, BasicBlock *BB,
                    unsigned NumOperands)
      : SeaMemoryAccess(C, Vty, DeleteValue, BB, NumOperands),
        MemoryInstruction(MI), OptimizedAccessAlias(MayAlias) {
    setDefiningAccess(DMA);
  }

  // Use deleteValue() to delete a generic MemoryUseOrDef.
  ~SeaMemoryUseOrDef() = default;

  void setOptimizedAccessType(Optional<AliasResult> AR) {
    OptimizedAccessAlias = AR;
  }

  void setDefiningAccess(SeaMemoryAccess *DMA, bool Optimized = false,
                         Optional<AliasResult> AR = MayAlias) {
    if (!Optimized) {
      setOperand(0, DMA);
      return;
    }
    setOptimized(DMA);
    setOptimizedAccessType(AR);
  }

private:
  Instruction *MemoryInstruction;
  Optional<AliasResult> OptimizedAccessAlias;
};

class SeaMemoryUse final : public SeaMemoryUseOrDef {
public:
  DECLARE_TRANSPARENT_OPERAND_ACCESSORS(SeaMemoryAccess);

  SeaMemoryUse(LLVMContext &C, SeaMemoryAccess *DMA, Instruction *MI,
               BasicBlock *BB)
      : SeaMemoryUseOrDef(C, DMA, MemoryUseVal, deleteMe, MI, BB,
                          /*NumOperands=*/1) {}

  // allocate space for exactly one operand
  void *operator new(size_t s) { return User::operator new(s, 1); }

  static bool classof(const Value *MA) {
    return MA->getValueID() == MemoryUseVal;
  }

  void print(raw_ostream &OS) const;

  void setOptimized(SeaMemoryAccess *DMA) {
    OptimizedID = DMA->getID();
    setOperand(0, DMA);
  }

  bool isOptimized() const {
    return getDefiningAccess() && OptimizedID == getDefiningAccess()->getID();
  }

  SeaMemoryAccess *getOptimized() const { return getDefiningAccess(); }

  void resetOptimized() { OptimizedID = INVALID_MEMORYACCESS_ID; }

protected:
  friend class SeaMemorySSA;

private:
  static void deleteMe(DerivedUser *Self);

  unsigned OptimizedID = INVALID_MEMORYACCESS_ID;
};

} // namespace seadsa

namespace llvm {

template <>
struct OperandTraits<seadsa::SeaMemoryUse>
    : public FixedNumOperandTraits<seadsa::SeaMemoryUse, 1> {};
} // namespace llvm

namespace seadsa {
DEFINE_TRANSPARENT_OPERAND_ACCESSORS(SeaMemoryUse, SeaMemoryAccess)

class SeaMemoryDef final : public SeaMemoryUseOrDef {
public:
  friend class SeaMemorySSA;

  DECLARE_TRANSPARENT_OPERAND_ACCESSORS(SeaMemoryAccess);

  SeaMemoryDef(LLVMContext &C, SeaMemoryAccess *DMA, Instruction *MI,
               BasicBlock *BB, unsigned Ver)
      : SeaMemoryUseOrDef(C, DMA, MemoryDefVal, deleteMe, MI, BB,
                          /*NumOperands=*/2),
        ID(Ver) {}

  // allocate space for exactly two operands
  void *operator new(size_t s) { return User::operator new(s, 2); }

  static bool classof(const Value *MA) {
    return MA->getValueID() == MemoryDefVal;
  }

  void setOptimized(SeaMemoryAccess *MA) {
    setOperand(1, MA);
    OptimizedID = MA->getID();
  }

  SeaMemoryAccess *getOptimized() const {
    return cast_or_null<SeaMemoryAccess>(getOperand(1));
  }

  bool isOptimized() const {
    return getOptimized() && OptimizedID == getOptimized()->getID();
  }

  void resetOptimized() {
    OptimizedID = INVALID_MEMORYACCESS_ID;
    setOperand(1, nullptr);
  }

  void print(raw_ostream &OS) const;

  unsigned getID() const { return ID; }

private:
  static void deleteMe(DerivedUser *Self);

  const unsigned ID;
  unsigned OptimizedID = INVALID_MEMORYACCESS_ID;
};
} // namespace seadsa

namespace llvm {
template <>
struct OperandTraits<seadsa::SeaMemoryDef>
    : public FixedNumOperandTraits<seadsa::SeaMemoryDef, 2> {};

template <> struct OperandTraits<seadsa::SeaMemoryUseOrDef> {
  static Use *op_begin(seadsa::SeaMemoryUseOrDef *MUD) {
    if (auto *MU = dyn_cast<seadsa::SeaMemoryUse>(MUD))
      return OperandTraits<seadsa::SeaMemoryUse>::op_begin(MU);
    return OperandTraits<seadsa::SeaMemoryDef>::op_begin(
        cast<seadsa::SeaMemoryDef>(MUD));
  }

  static Use *op_end(seadsa::SeaMemoryUseOrDef *MUD) {
    if (auto *MU = dyn_cast<seadsa::SeaMemoryUse>(MUD))
      return OperandTraits<seadsa::SeaMemoryUse>::op_end(MU);
    return OperandTraits<seadsa::SeaMemoryDef>::op_end(
        cast<seadsa::SeaMemoryDef>(MUD));
  }

  static unsigned operands(const seadsa::SeaMemoryUseOrDef *MUD) {
    if (const auto *MU = dyn_cast<seadsa::SeaMemoryUse>(MUD))
      return OperandTraits<seadsa::SeaMemoryUse>::operands(MU);
    return OperandTraits<seadsa::SeaMemoryDef>::operands(
        cast<seadsa::SeaMemoryDef>(MUD));
  }
};
} // namespace llvm

namespace seadsa {
DEFINE_TRANSPARENT_OPERAND_ACCESSORS(SeaMemoryDef, SeaMemoryAccess)
DEFINE_TRANSPARENT_OPERAND_ACCESSORS(SeaMemoryUseOrDef, SeaMemoryAccess)

class SeaMemoryPhi final : public SeaMemoryAccess {
  // allocate space for exactly zero operands
  void *operator new(size_t s) { return User::operator new(s); }

public:
  /// Provide fast operand accessors
  DECLARE_TRANSPARENT_OPERAND_ACCESSORS(SeaMemoryAccess);

  SeaMemoryPhi(LLVMContext &C, BasicBlock *BB, unsigned Ver,
               unsigned NumPreds = 0)
      : SeaMemoryAccess(C, MemoryPhiVal, deleteMe, BB, 0), ID(Ver),
        ReservedSpace(NumPreds) {
    allocHungoffUses(ReservedSpace);
  }

  // Block iterator interface. This provides access to the list of incoming
  // basic blocks, which parallels the list of incoming values.
  using block_iterator = BasicBlock **;
  using const_block_iterator = BasicBlock *const *;

  block_iterator block_begin() {
    auto *Ref = reinterpret_cast<Use::UserRef *>(op_begin() + ReservedSpace);
    return reinterpret_cast<block_iterator>(Ref + 1);
  }

  const_block_iterator block_begin() const {
    const auto *Ref =
        reinterpret_cast<const Use::UserRef *>(op_begin() + ReservedSpace);
    return reinterpret_cast<const_block_iterator>(Ref + 1);
  }

  block_iterator block_end() { return block_begin() + getNumOperands(); }

  const_block_iterator block_end() const {
    return block_begin() + getNumOperands();
  }

  iterator_range<block_iterator> blocks() {
    return make_range(block_begin(), block_end());
  }

  iterator_range<const_block_iterator> blocks() const {
    return make_range(block_begin(), block_end());
  }

  op_range incoming_values() { return operands(); }

  const_op_range incoming_values() const { return operands(); }

  /// Return the number of incoming edges
  unsigned getNumIncomingValues() const { return getNumOperands(); }

  /// Return incoming value number x
  SeaMemoryAccess *getIncomingValue(unsigned I) const { return getOperand(I); }
  void setIncomingValue(unsigned I, SeaMemoryAccess *V) { setOperand(I, V); }

  static unsigned getOperandNumForIncomingValue(unsigned I) { return I; }
  static unsigned getIncomingValueNumForOperand(unsigned I) { return I; }

  /// Return incoming basic block number @p i.
  BasicBlock *getIncomingBlock(unsigned I) const { return block_begin()[I]; }

  /// Return incoming basic block corresponding
  /// to an operand of the PHI.
  BasicBlock *getIncomingBlock(const Use &U) const {
    assert(this == U.getUser() && "Iterator doesn't point to PHI's Uses?");
    return getIncomingBlock(unsigned(&U - op_begin()));
  }

  /// Return incoming basic block corresponding
  /// to value use iterator.
  BasicBlock *getIncomingBlock(SeaMemoryAccess::const_user_iterator I) const {
    return getIncomingBlock(I.getUse());
  }

  void setIncomingBlock(unsigned I, BasicBlock *BB) {
    assert(BB && "PHI node got a null basic block!");
    block_begin()[I] = BB;
  }

  /// Add an incoming value to the end of the PHI list
  void addIncoming(SeaMemoryAccess *V, BasicBlock *BB) {
    if (getNumOperands() == ReservedSpace) growOperands(); // Get more space!
    // Initialize some new operands.
    setNumHungOffUseOperands(getNumOperands() + 1);
    setIncomingValue(getNumOperands() - 1, V);
    setIncomingBlock(getNumOperands() - 1, BB);
  }

  /// Return the first index of the specified basic
  /// block in the value list for this PHI.  Returns -1 if no instance.
  int getBasicBlockIndex(const BasicBlock *BB) const {
    for (unsigned I = 0, E = getNumOperands(); I != E; ++I)
      if (block_begin()[I] == BB) return I;
    return -1;
  }

  SeaMemoryAccess *getIncomingValueForBlock(const BasicBlock *BB) const {
    int Idx = getBasicBlockIndex(BB);
    assert(Idx >= 0 && "Invalid basic block argument!");
    return getIncomingValue(Idx);
  }

  // After deleting incoming position I, the order of incoming may be changed.
  void unorderedDeleteIncoming(unsigned I) {
    unsigned E = getNumOperands();
    assert(I < E && "Cannot remove out of bounds Phi entry.");
    // MemoryPhi must have at least two incoming values, otherwise the MemoryPhi
    // itself should be deleted.
    assert(E >= 2 && "Cannot only remove incoming values in MemoryPhis with "
                     "at least 2 values.");
    setIncomingValue(I, getIncomingValue(E - 1));
    setIncomingBlock(I, block_begin()[E - 1]);
    setOperand(E - 1, nullptr);
    block_begin()[E - 1] = nullptr;
    setNumHungOffUseOperands(getNumOperands() - 1);
  }

  // After deleting entries that satisfy Pred, remaining entries may have
  // changed order.
  template <typename Fn> void unorderedDeleteIncomingIf(Fn &&Pred) {
    for (unsigned I = 0, E = getNumOperands(); I != E; ++I)
      if (Pred(getIncomingValue(I), getIncomingBlock(I))) {
        unorderedDeleteIncoming(I);
        E = getNumOperands();
        --I;
      }
    assert(getNumOperands() >= 1 &&
           "Cannot remove all incoming blocks in a MemoryPhi.");
  }

  // After deleting incoming block BB, the incoming blocks order may be changed.
  void unorderedDeleteIncomingBlock(const BasicBlock *BB) {
    unorderedDeleteIncomingIf(
        [&](const SeaMemoryAccess *, const BasicBlock *B) { return BB == B; });
  }

  // After deleting incoming memory access MA, the incoming accesses order may
  // be changed.
  void unorderedDeleteIncomingValue(const SeaMemoryAccess *MA) {
    unorderedDeleteIncomingIf(
        [&](const SeaMemoryAccess *M, const BasicBlock *) { return MA == M; });
  }

  static bool classof(const Value *V) {
    return V->getValueID() == MemoryPhiVal;
  }

  void print(raw_ostream &OS) const;

  unsigned getID() const { return ID; }

protected:
  friend class SeaMemorySSA;

  /// this is more complicated than the generic
  /// User::allocHungoffUses, because we have to allocate Uses for the incoming
  /// values and pointers to the incoming blocks, all in one allocation.
  void allocHungoffUses(unsigned N) {
    User::allocHungoffUses(N, /* IsPhi */ true);
  }

private:
  // For debugging only
  const unsigned ID;
  unsigned ReservedSpace;

  /// This grows the operand list in response to a push_back style of
  /// operation.  This grows the number of ops by 1.5 times.
  void growOperands() {
    unsigned E = getNumOperands();
    // 2 op PHI nodes are VERY common, so reserve at least enough for that.
    ReservedSpace = std::max(E + E / 2, 2u);
    growHungoffUses(ReservedSpace, /* IsPhi */ true);
  }

  static void deleteMe(DerivedUser *Self);
};

inline unsigned SeaMemoryAccess::getID() const {
  assert((isa<SeaMemoryDef>(this) || isa<SeaMemoryPhi>(this)) &&
         "only memory defs and phis have ids");
  if (const auto *MD = dyn_cast<SeaMemoryDef>(this)) return MD->getID();
  return cast<SeaMemoryPhi>(this)->getID();
}

inline bool SeaMemoryUseOrDef::isOptimized() const {
  if (const auto *MD = dyn_cast<SeaMemoryDef>(this)) return MD->isOptimized();
  return cast<SeaMemoryUse>(this)->isOptimized();
}

inline SeaMemoryAccess *SeaMemoryUseOrDef::getOptimized() const {
  if (const auto *MD = dyn_cast<SeaMemoryDef>(this)) return MD->getOptimized();
  return cast<SeaMemoryUse>(this)->getOptimized();
}

inline void SeaMemoryUseOrDef::setOptimized(SeaMemoryAccess *MA) {
  if (auto *MD = dyn_cast<SeaMemoryDef>(this))
    MD->setOptimized(MA);
  else
    cast<SeaMemoryUse>(this)->setOptimized(MA);
}

inline void SeaMemoryUseOrDef::resetOptimized() {
  if (auto *MD = dyn_cast<SeaMemoryDef>(this))
    MD->resetOptimized();
  else
    cast<SeaMemoryUse>(this)->resetOptimized();
}
} // namespace seadsa

namespace llvm {
template <>
struct OperandTraits<seadsa::SeaMemoryPhi> : public HungoffOperandTraits<2> {};
} // namespace llvm

namespace seadsa {
DEFINE_TRANSPARENT_OPERAND_ACCESSORS(SeaMemoryPhi, SeaMemoryAccess)
}

namespace seadsa {
class ShadowMem;

enum class ShadowMemInstOp {
  LOAD,        /* load */
  TRSFR_LOAD,  /* memory transfer load */
  STORE,       /* store */
  GLOBAL_INIT, /* initialization of global values */
  INIT,        /* initialization of local shadow variables */
  ARG_INIT,    /* initialization of shadow formal parameters */
  ARG_REF,     /* input actual parameter */
  ARG_MOD,     /* input/output actual parameter */
  ARG_NEW,     /* output actual parameter */
  FUN_IN,      /* input formal parameter */
  FUN_OUT,     /* output formal parameter */
  UNKNOWN
};

/// MemorySSA implementation backed up by SeaDsa
///
/// Unlike llvm::MemorySSA, there are multiple memory regions per Module, but
/// there is still only one region per instruction.
class SeaMemorySSA {
public:
  SeaMemorySSA(llvm::Function &, seadsa::GlobalAnalysis &);
  SeaMemorySSA(SeaMemorySSA &&) = delete;
  ~SeaMemorySSA();

  SeaMemoryUseOrDef *getMemoryAccess(const Instruction *I) const {
    return cast_or_null<SeaMemoryUseOrDef>(ValueToMemoryAccess.lookup(I));
  }

  SeaMemoryPhi *getMemoryAccess(const BasicBlock *BB) const {
    return cast_or_null<SeaMemoryPhi>(
        ValueToMemoryAccess.lookup(cast<Value>(BB)));
  }

  void dump() const;
  void print(raw_ostream &) const;

  using AccessList =
      iplist<SeaMemoryAccess, ilist_tag<SMSSAHelpers::AllAccessTag>>;

  using iterator = AccessList::iterator;
  using const_iterator = AccessList::const_iterator;

  const AccessList *getBlockAccesses(const BasicBlock *BB) const {
    return getWritableBlockAccesses(BB);
  }

  const AccessList *getLiveOnEntry() const { return LiveOnEntryDefs.get(); }
  const AccessList *getLiveOnExit() const { return LiveOnExitUses.get(); }

  iterator_range<const_iterator> liveOnEntry() const {
    return llvm::make_range(LiveOnEntryDefs->begin(), LiveOnEntryDefs->end());
  }
  iterator_range<const_iterator> liveOnExit() const {
    return llvm::make_range(LiveOnExitUses->begin(), LiveOnExitUses->end());
  }

protected:
  // This is used by the use optimizer and updater.
  AccessList *getWritableBlockAccesses(const BasicBlock *BB) const {
    auto It = PerBlockAccesses.find(BB);
    return It == PerBlockAccesses.end() ? nullptr : It->second.get();
  }

  AccessList *getOrCreateAccessList(const BasicBlock *BB);
  void buildForFunction(ShadowMem &);

private:
  Function &F;
  seadsa::GlobalAnalysis &DSA;

  // Memory SSA mappings
  DenseMap<const Value *, SeaMemoryAccess *> ValueToMemoryAccess;

  using AccessMap = DenseMap<const BasicBlock *, std::unique_ptr<AccessList>>;
  AccessMap PerBlockAccesses;

  std::unique_ptr<AccessList> LiveOnEntryDefs;
  std::unique_ptr<AccessList> LiveOnExitUses;

  unsigned NextID;

  friend class ShadowMem;
  friend class ShadowMemImpl;
};

class ShadowMem {
  std::unique_ptr<ShadowMemImpl> m_impl;

public:
  ShadowMem(GlobalAnalysis &dsa, AllocSiteInfo &asi,
            llvm::TargetLibraryInfoWrapperPass &tli, llvm::CallGraph *cg,
            llvm::Pass &pass /* for dominatorTree and assumptionCache*/,
            bool splitDsaNodes = false, bool computeReadMod = false,
            bool memOptimizer = true, bool useTBAA = true);

  bool runOnModule(llvm::Module &M);

  // Return a reference to the global sea-dsa analysis.
  GlobalAnalysis &getDsaAnalysis();

  // Return true if Dsa nodes are split by fields (i.e., whether
  // ShadowMem is field-sensitivity or not)
  bool splitDsaNodes() const;

  // Return the id of the field pointed by the given cell c.
  llvm::Optional<unsigned> getCellId(const Cell &c) const;

  ShadowMemInstOp getShadowMemOp(const llvm::CallInst &ci) const;

  // Return cell associated to the shadow mem call instruction.
  llvm::Optional<Cell> getShadowMemCell(const llvm::CallInst &ci) const;

  // Return a pair <def,use> with the defined and used variable by the
  // shadow mem instruction. If the instruction does not define or use
  // a variable the corresponding pair element can be null.
  std::pair<llvm::Value *, llvm::Value *>
  getShadowMemVars(llvm::CallInst &ci) const;

  SeaMemorySSA *getMemorySSA(llvm::Function &F);
};

class ShadowMemPass : public llvm::ModulePass {
  std::unique_ptr<ShadowMem> m_shadowMem;

public:
  static char ID;
  ShadowMemPass();
  bool runOnModule(llvm::Module &M) override;
  void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;
  llvm::StringRef getPassName() const override { return "ShadowMemSeaDsa"; }
  const ShadowMem &getShadowMem() const;
  ShadowMem &getShadowMem();
};

class StripShadowMemPass : public llvm::ModulePass {
public:
  static char ID;
  StripShadowMemPass() : llvm::ModulePass(ID) {}
  virtual bool runOnModule(llvm::Module &M) override;
  virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;
  virtual llvm::StringRef getPassName() const { return "StripShadowMem"; }
};

llvm::Pass *createStripShadowMemPass();
llvm::Pass *createShadowMemPass();

bool isShadowMemInst(const llvm::Value &v);
} // namespace seadsa
