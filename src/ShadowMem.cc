#include "sea_dsa/ShadowMem.hh"

#include "sea_dsa/AllocSiteInfo.hh"
#include "sea_dsa/CallSite.hh"
#include "sea_dsa/DsaAnalysis.hh"
#include "sea_dsa/Mapper.hh"
#include "sea_dsa/TypeUtils.hh"
#include "sea_dsa/support/Debug.h"

#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/ADT/SCCIterator.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/BasicAliasAnalysis.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/MemoryBuiltins.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Analysis/TypeBasedAliasAnalysis.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/Local.h"
#include "llvm/Transforms/Utils/PromoteMemToReg.h"

#include "boost/range/algorithm/set_algorithm.hpp"

llvm::cl::opt<bool> SplitFields("horn-sea-dsa-split",
                                llvm::cl::desc("DSA: Split nodes by fields"),
                                llvm::cl::init(false));

llvm::cl::opt<bool>
    LocalReadMod("horn-sea-dsa-local-mod",
                 llvm::cl::desc("DSA: Compute read/mod info locally"),
                 llvm::cl::init(false));

llvm::cl::opt<bool> ShadowMemOptimize(
    "horn-shadow-mem-optimize",
    llvm::cl::desc("Use the solved form for ShadowMem (MemSSA)"),
    llvm::cl::init(true));

llvm::cl::opt<bool>
    ShadowMemUseTBAA("horn-shadow-mem-use-tbaa",
                     llvm::cl::desc("Use TypeBasedAA in the MemSSA optimizer"),
                     llvm::cl::init(true));

using namespace llvm;
namespace dsa = sea_dsa;
namespace {

bool HasReturn(BasicBlock &bb, ReturnInst *&retInst) {
  if (auto *ret = dyn_cast<ReturnInst>(bb.getTerminator())) {
    retInst = ret;
    return true;
  }
  return false;
}

bool HasReturn(Function &F, ReturnInst *&retInst) {
  for (auto &bb : F) {
    if (HasReturn(bb, retInst))
      return true;
  }
  return false;
}

/// work around bug in llvm::RecursivelyDeleteTriviallyDeadInstructions
bool recursivelyDeleteTriviallyDeadInstructions(
    llvm::Value *V, const llvm::TargetLibraryInfo *TLI = nullptr) {

  Instruction *I = dyn_cast<Instruction>(V);
  if (!I->getParent())
    return false;
  return llvm::RecursivelyDeleteTriviallyDeadInstructions(V, TLI);
}

Value *getUniqueScalar(LLVMContext &ctx, IRBuilder<> &B, const dsa::Cell &c) {
  const dsa::Node *n = c.getNode();
  if (n && c.getOffset() == 0) {
    Value *v = const_cast<Value *>(n->getUniqueScalar());

    // -- a unique scalar is a single-cell global variable. We might be
    // -- able to extend this to single-cell local pointers, but these
    // -- are probably not very common.
    if (auto *gv = dyn_cast_or_null<GlobalVariable>(v))
      if (gv->getType()->getElementType()->isSingleValueType())
        return B.CreateBitCast(v, Type::getInt8PtrTy(ctx));
  }
  return ConstantPointerNull::get(Type::getInt8PtrTy(ctx));
}

template <typename Set> void markReachableNodes(const dsa::Node *n, Set &set) {
  if (!n)
    return;
  assert(!n->isForwarding() && "Cannot mark a forwarded node");

  if (set.insert(n).second)
    for (const auto &edg : n->links())
      markReachableNodes(edg.second->getNode(), set);
}

template <typename Set>
void reachableNodes(const Function &fn, dsa::Graph &g, Set &inputReach,
                    Set &retReach) {
  // formal parameters
  for (const auto &formal : fn.args())
    if (g.hasCell(formal)) {
      dsa::Cell &c = g.mkCell(formal, dsa::Cell());
      markReachableNodes(c.getNode(), inputReach);
    }

  // globals
  for (auto &kv : llvm::make_range(g.globals_begin(), g.globals_end()))
    markReachableNodes(kv.second->getNode(), inputReach);

  // return value
  if (g.hasRetCell(fn))
    markReachableNodes(g.getRetCell(fn).getNode(), retReach);
}

template <typename Set> void set_difference(Set &s1, Set &s2) {
  Set s3;
  boost::set_difference(s1, s2, std::inserter(s3, s3.end()));
  std::swap(s3, s1);
}

template <typename Set> void set_union(Set &s1, Set &s2) {
  Set s3;
  boost::set_union(s1, s2, std::inserter(s3, s3.end()));
  std::swap(s3, s1);
}

/// Computes Node reachable from the call arguments in the graph.
/// reach - all reachable nodes
/// outReach - subset of reach that is only reachable from the return node
template <typename Set1, typename Set2>
void argReachableNodes(const Function &fn, dsa::Graph &G, Set1 &reach,
                       Set2 &outReach) {
  reachableNodes(fn, G, reach, outReach);
  set_difference(outReach, reach);
  set_union(reach, outReach);
}

/// Interface used to query BasicAA and TBAA, if available. This should ideally
/// be replaced with llvm::AAResultsWrapperPass, but we hit crashes when calling
/// alias -- this class is just a simple workaround.
/// TODO: Investigate crashes in llvm::AAResultsWrapperPass.
class LocalAAResultsWrapper {
  llvm::BasicAAResult *m_baa = nullptr;
  llvm::TypeBasedAAResult *m_tbaa = nullptr;

  MemoryLocation getMemLoc(Value &ptr, Value *inst, Optional<unsigned> size) {
    MemoryLocation loc(&ptr);
    if (auto *i = dyn_cast_or_null<Instruction>(inst)) {
      AAMDNodes aaTags;
      i->getAAMetadata(aaTags);
      loc = MemoryLocation(&ptr, MemoryLocation::UnknownSize, aaTags);
    }

    if (size.hasValue())
      loc = loc.getWithNewSize(*size);

    return loc;
  }

public:
  LocalAAResultsWrapper() = default;
  LocalAAResultsWrapper(BasicAAResult *baa, TypeBasedAAResult *tbaa)
      : m_baa(baa), m_tbaa(tbaa) {}
  LocalAAResultsWrapper(const LocalAAResultsWrapper &) = default;
  LocalAAResultsWrapper &operator=(const LocalAAResultsWrapper &) = default;

  bool isNoAlias(Value &ptrA, Value *instA, Optional<unsigned> sizeA,
                 Value &ptrB, Value *instB, Optional<unsigned> sizeB) {
    if (!m_baa && !m_tbaa)
      return false;

    MemoryLocation A = getMemLoc(ptrA, instA, sizeA);
    MemoryLocation B = getMemLoc(ptrB, instB, sizeB);

    if (m_tbaa && m_tbaa->alias(A, B) == AliasResult::NoAlias)
      return true;

    return m_baa && m_baa->alias(A, B) == AliasResult::NoAlias;
  }
};
} // end namespace

namespace sea_dsa {

bool isShadowMemInst(const llvm::Value &v) {
  if (auto *inst = dyn_cast<const Instruction>(&v)) {
    return inst->getMetadata("shadow.mem");
  }
  return false;
}

class ShadowMemImpl : public InstVisitor<ShadowMemImpl> {
  dsa::GlobalAnalysis &m_dsa;
  TargetLibraryInfo &m_tli;
  const DataLayout *m_dl;
  CallGraph *m_callGraph;
  Pass &m_pass;

  bool m_splitDsaNodes;
  bool m_computeReadMod;
  bool m_memOptimizer;
  bool m_useTBAA;

  llvm::Constant *m_memLoadFn = nullptr;
  llvm::Constant *m_memStoreFn = nullptr;
  llvm::Constant *m_memTrsfrLoadFn = nullptr;
  llvm::Constant *m_memShadowInitFn = nullptr;
  llvm::Constant *m_memShadowArgInitFn = nullptr;
  llvm::Constant *m_argRefFn = nullptr;
  llvm::Constant *m_argModFn = nullptr;
  llvm::Constant *m_argNewFn = nullptr;
  llvm::Constant *m_markIn = nullptr;
  llvm::Constant *m_markOut = nullptr;
  llvm::Constant *m_memGlobalVarInitFn = nullptr;

  llvm::SmallVector<llvm::Constant *, 5> m_memInitFunctions;

  struct GlobalVariableOrdering {
    bool operator()(const GlobalVariable *gv1,
                    const GlobalVariable *gv2) const {
      assert(gv1->hasName());
      assert(gv2->hasName());
      return (gv1->getName() < gv2->getName());
    }
  };

  struct NodeOrdering {
    bool operator()(const dsa::Node *n1, const dsa::Node *n2) const {
      return n1->getId() < n2->getId();
    }
  };

  // We use std::map to keep keys ordered.
  using ShadowsMap =
      std::map<const dsa::Node *, std::map<unsigned, llvm::AllocaInst *>,
               NodeOrdering>;

  // using ShadowsMap =
  //     llvm::DenseMap<const dsa::Node *,
  //                    llvm::DenseMap<unsigned, llvm::AllocaInst *>>;

  using NodeIdMap = llvm::DenseMap<const dsa::Node *, unsigned>;

  /// \brief A map from DsaNode to all the shadow pseudo-variable corresponding
  /// to it
  ///
  /// If \f m_splitDsaNodes is false, each DsaNode has a single shadow variable
  /// If
  /// \f m_splitDsaNodes is true, each field in the node has a shadow variable.
  /// Thus
  /// the map connects a node to a pair of an offset and an AllocaInst that
  /// corresponds to the shadow variable
  ShadowsMap m_shadows;

  /// \brief A map from DsaNode to its numeric id
  NodeIdMap m_nodeIds;

  // Associate a shadow instruction to its Cell. Used by ShadowMem
  // clients (e.g., VCGen).
  llvm::DenseMap<const CallInst *, dsa::Cell> m_shadowMemInstToCell;

  /// \brief The largest id used so far. Used to allocate fresh ids
  unsigned m_maxId = 0;

  /// \brief A pointer to i32 type for convenience
  llvm::Type *m_Int32Ty;

  /// Used by doReadMod
  using NodeSet = boost::container::flat_set<const dsa::Node *>;
  using NodeSetMap = llvm::DenseMap<const llvm::Function *, NodeSet>;
  /// \brief A map from Function to all DsaNode that are read by it
  NodeSetMap m_readList;
  /// \brief A map from Function to all DsaNode that are written by it
  NodeSetMap m_modList;

  // -- temporaries, used by visitor
  llvm::LLVMContext *m_llvmCtx = nullptr;
  std::unique_ptr<IRBuilder<>> m_B = nullptr;
  dsa::Graph *m_graph = nullptr;
  LocalAAResultsWrapper m_localAAResults;

  // Associate shadow and concrete instructions, used by the memUse solver.
  // Alternatively, this could be implemented by walking instructions forward,
  // starting from a shadow instructions, until the first concrete instructions
  // is found, or by using some metadata.
  llvm::DenseMap<CallInst *, Value *> m_shadowOpToAccessedPtr;
  llvm::DenseMap<CallInst *, Value *> m_shadowOpToConcreteInst;
  void associateConcretePtr(CallInst &memOp, Value &ptr, Value *inst);
  Value *getAssociatedConcretePtr(CallInst &memOp);
  Value *getAssociatedConcreteInst(CallInst &memOp);

  /// \brief Create shadow pseudo-functions in the module
  void mkShadowFunctions(Module &M);

  /// \brief Re-compute read/mod information for each function
  ///
  /// This only makes sense for context-insensitive SeaDsa under which all
  /// functions share the same DsaGraph. In this case, the Mod/Ref information
  /// in the graph is common to all functions. This procedure recomputes the
  /// Mod/Ref information locally by traversing all the functions bottom up in
  /// the call graph order.
  ///
  /// The procedure is unsound if used together with context-sensitive mode of
  /// SeaDsa
  void doReadMod();

  /// \brief Computes Mod/Ref sets for the given function \p F
  void updateReadMod(Function &F, NodeSet &readSet, NodeSet &modSet);

  /// \brief Returns the offset of the field pointed by \p c
  ///
  /// Returns 0 if \f m_splitDsaNodes is false
  unsigned getOffset(const dsa::Cell &c) const {
    return m_splitDsaNodes ? c.getOffset() : 0;
  }
  
  /// \breif Returns a local id of a given field of DsaNode \p n
  unsigned getFieldId(const dsa::Node &n, unsigned offset) {
    auto it = m_nodeIds.find(&n);
    if (it != m_nodeIds.end())
      return it->second + offset;

    // allocate new id for the node
    unsigned id = m_maxId;
    m_nodeIds[&n] = id;
    if (n.size() == 0) {
      // XXX: what does it mean for a node to have 0 size?
      assert(offset == 0);
      ++m_maxId;
      return id;
    }

    // -- allocate enough ids for the node and all its fields
    m_maxId += n.size();
    return id + offset;
  }

  /// \breif Returns id of a field pointed to by the given cell \c
  unsigned getFieldId(const dsa::Cell &c) {
    assert(c.getNode());
    return getFieldId(*c.getNode(), getOffset(c));
  }

  /// \brief Returns shadow variable for a given field
  AllocaInst *getShadowForField(const dsa::Node &n, unsigned offset) {
    auto &offsetMap = m_shadows[&n];
    auto it = offsetMap.find(offset);
    if (it != offsetMap.end())
      return it->second;

    // -- not found, allocate new shadow variable
    AllocaInst *inst = new AllocaInst(m_Int32Ty, 0 /* Address Space */);
    // -- inst is eventually added to the Module and it will take ownership
    offsetMap[offset] = inst;
    return inst;
  }

  /// \breif Returns shadow variable for a field pointed to by a cell \p cell
  AllocaInst *getShadowForField(const dsa::Cell &cell) {
    return getShadowForField(*cell.getNode(), getOffset(cell));
    assert(cell.getNode());
  }

  bool isRead(const dsa::Cell &c, const Function &f) {
    return c.getNode() ? isRead(c.getNode(), f) : false;
  }

  bool isModified(const dsa::Cell &c, const Function &f) {
    return c.getNode() ? isModified(c.getNode(), f) : false;
  }

  bool isRead(const dsa::Node *n, const Function &f) {
    LOG("shadow_mod",
        if (m_computeReadMod && n->isRead() != (m_readList[&f].count(n) > 0)) {
          errs() << f.getName() << " readNode: " << n->isRead()
                 << " readList: " << m_readList[&f].count(n) << "\n";
          if (n->isRead())
            n->write(errs());
        });
    return m_computeReadMod ? m_readList[&f].count(n) > 0 : n->isRead();
  }

  bool isModified(const dsa::Node *n, const Function &f) {
    LOG("shadow_mod", if (m_computeReadMod &&
                          n->isModified() != (m_modList[&f].count(n) > 0)) {
      errs() << f.getName() << " modNode: " << n->isModified()
             << " modList: " << m_modList[&f].count(n) << "\n";
      if (n->isModified())
        n->write(errs());
    });
    return m_computeReadMod ? m_modList[&f].count(n) > 0 : n->isModified();
  }

  MDNode *mkMetaConstant(llvm::Optional<unsigned> val) {
    assert(m_llvmCtx);
    if (val.hasValue())
      return MDNode::get(*m_llvmCtx,
                         ConstantAsMetadata::get(ConstantInt::get(
                             *m_llvmCtx, llvm::APInt(64u, size_t(*val)))));

    return MDNode::get(*m_llvmCtx, llvm::None);
  }

  Optional<unsigned> maybeGetMetaConstant(CallInst &memOp, StringRef metaName) {
    if (MDNode *meta = memOp.getMetadata(metaName))
      if (meta->getNumOperands() > 0)
        if (auto *c = dyn_cast<ConstantAsMetadata>(meta->getOperand(0)))
          if (auto *cInt = dyn_cast<ConstantInt>(c->getValue()))
            return unsigned(cInt->getLimitedValue());

    return llvm::None;
  }

  void markDefCall(CallInst *ci, llvm::Optional<unsigned> accessedBytes) {
    assert(m_llvmCtx);
    MDNode *meta = MDNode::get(*m_llvmCtx, None);
    ci->setMetadata(m_metadataTag, meta);
    ci->setMetadata(m_memDefTag, mkMetaConstant(accessedBytes));
  }

  void markUseCall(CallInst *ci, llvm::Optional<unsigned> accessedBytes) {
    assert(m_llvmCtx);
    MDNode *meta = MDNode::get(*m_llvmCtx, None);
    ci->setMetadata(m_metadataTag, meta);
    ci->setMetadata(m_memUseTag, mkMetaConstant(accessedBytes));
  }

  CallInst *mkShadowCall(IRBuilder<> &B, const dsa::Cell &c, Constant *fn,
                         ArrayRef<Value *> args, const Twine &name = "") {
    CallInst *ci = B.CreateCall(fn, args, name);
    m_shadowMemInstToCell.insert({ci, dsa::Cell(c.getNode(), c.getOffset())});
    return ci;
  }

  CallInst &mkShadowAllocInit(IRBuilder<> &B, Constant *fn, AllocaInst *a,
                              const dsa::Cell &c) {
    B.Insert(a, "shadow.mem." + Twine(getFieldId(c)));
    CallInst *ci;
    Value *us = getUniqueScalar(*m_llvmCtx, B, c);
    ci = mkShadowCall(B, c, fn, {B.getInt32(getFieldId(c)), us}, "sm");
    markDefCall(ci, llvm::None);
    B.CreateStore(ci, a);
    return *ci;
  }

  CallInst &mkShadowStore(IRBuilder<> &B, const dsa::Cell &c,
                          llvm::Optional<unsigned> bytes) {
    AllocaInst *v = getShadowForField(c);
    auto &ci = mkStoreFnCall(B, c, v, bytes);
    B.CreateStore(&ci, v);
    return ci;
  }

  CallInst &mkStoreFnCall(IRBuilder<> &B, const dsa::Cell &c, AllocaInst *v,
                          llvm::Optional<unsigned> bytes) {

    auto *ci = mkShadowCall(B, c, m_memStoreFn,
                            {m_B->getInt32(getFieldId(c)), m_B->CreateLoad(v),
                             getUniqueScalar(*m_llvmCtx, B, c)},
                            "sm");
    markDefCall(ci, bytes);
    return *ci;
  }

  CallInst *mkShadowGlobalVarInit(IRBuilder<> &B, const dsa::Cell &c,
                                  llvm::GlobalVariable &_u,
                                  llvm::Optional<unsigned> bytes) {

    // Do not insert shadow.mem.global.init() if the global is a unique scalar
    // Such scalars are initialized directly in the code
    Value *scalar = getUniqueScalar(*m_llvmCtx, B, c);
    if (!isa<ConstantPointerNull>(scalar))
      return nullptr;

    Value *u = B.CreateBitCast(&_u, Type::getInt8PtrTy(*m_llvmCtx));
    AllocaInst *v = getShadowForField(c);
    auto *ci = mkShadowCall(
        B, c, m_memGlobalVarInitFn,
        {m_B->getInt32(getFieldId(c)), m_B->CreateLoad(v), u}, "sm");
    markDefCall(ci, bytes);
    B.CreateStore(ci, v);
    return ci;
  }

  CallInst &mkShadowLoad(IRBuilder<> &B, const dsa::Cell &c,
                         llvm::Optional<unsigned> bytes) {
    auto *ci = mkShadowCall(B, c, m_memLoadFn,
                            {B.getInt32(getFieldId(c)),
                             B.CreateLoad(getShadowForField(c)),
                             getUniqueScalar(*m_llvmCtx, B, c)});
    markUseCall(ci, bytes);
    return *ci;
  }

  std::pair<CallInst &, CallInst &>
  mkShadowMemTrsfr(IRBuilder<> &B, const dsa::Cell &dst, const dsa::Cell &src,
                   llvm::Optional<unsigned> bytes) {
    // insert memtrfr.load for the read access
    auto *loadCI = mkShadowCall(B, src, m_memTrsfrLoadFn,
                                {B.getInt32(getFieldId(src)),
                                 B.CreateLoad(getShadowForField(src)),
                                 getUniqueScalar(*m_llvmCtx, B, src)});
    markUseCall(loadCI, bytes);

    // insert normal mem.store for the write access
    auto &storeCI = mkShadowStore(B, dst, bytes);
    return {*loadCI, storeCI};
  }

  CallInst &mkArgRef(IRBuilder<> &B, const dsa::Cell &c, unsigned idx,
                     llvm::Optional<unsigned> bytes) {
    AllocaInst *v = getShadowForField(c);
    unsigned id = getFieldId(c);
    auto *ci =
        mkShadowCall(B, c, m_argRefFn,
                     {B.getInt32(id), m_B->CreateLoad(v), m_B->getInt32(idx),
                      getUniqueScalar(*m_llvmCtx, B, c)});
    markUseCall(ci, bytes);
    return *ci;
  }

  CallInst &mkArgNewMod(IRBuilder<> &B, Constant *argFn, const dsa::Cell &c,
                        unsigned idx, llvm::Optional<unsigned> bytes) {
    AllocaInst *v = getShadowForField(c);
    unsigned id = getFieldId(c);

    auto *ci = mkShadowCall(B, c, argFn,
                            {B.getInt32(id), B.CreateLoad(v), B.getInt32(idx),
                             getUniqueScalar(*m_llvmCtx, B, c)},
                            "sh");

    B.CreateStore(ci, v);
    markDefCall(ci, bytes);
    return *ci;
  }

  CallInst &mkMarkIn(IRBuilder<> &B, const dsa::Cell &c, Value *v, unsigned idx,
                     llvm::Optional<unsigned> bytes) {
    auto *ci = mkShadowCall(B, c, m_markIn,
                            {B.getInt32(getFieldId(c)), v, B.getInt32(idx),
                             getUniqueScalar(*m_llvmCtx, B, c)});
    markDefCall(ci, bytes);
    return *ci;
  }

  CallInst &mkMarkOut(IRBuilder<> &B, const dsa::Cell &c, unsigned idx,
                      llvm::Optional<unsigned> bytes) {
    auto *ci = mkShadowCall(
        B, c, m_markOut,
        {B.getInt32(getFieldId(c)), B.CreateLoad(getShadowForField(c)),
         B.getInt32(idx), getUniqueScalar(*m_llvmCtx, B, c)});
    markUseCall(ci, bytes);
    return *ci;
  }

  void runMem2Reg(Function &F) {
    std::vector<AllocaInst *> allocas;

    // -- for every node
    for (auto &kv : m_shadows) {
      // -- for every offset
      for (auto &entry : kv.second) {
        // -- only take allocas that are in some basic block
        if (entry.second->getParent())
          allocas.push_back(entry.second);
      }
    }

    DominatorTree &DT =
        m_pass.getAnalysis<llvm::DominatorTreeWrapperPass>(F).getDomTree();
    AssumptionCache &AC =
        m_pass.getAnalysis<llvm::AssumptionCacheTracker>().getAssumptionCache(
            F);
    PromoteMemToReg(allocas, DT, &AC);
  }

  /// \brief Infer which PHINodes corresponds to shadow pseudo-assignments
  /// The type is stored as a meta-data on the node
  void inferTypeOfPHINodes(Function &F) {
    // for every bb in topological order
    for (auto *bb : llvm::inverse_post_order(&F.getEntryBlock())) {
      // for every PHINode
      for (auto &phi : bb->phis()) {
        // for every incoming value
        for (auto &val : phi.incoming_values()) {
          // if an incoming value has metadata, take it, and be done
          if (auto *inst = dyn_cast<Instruction>(&val)) {
            if (auto *meta = inst->getMetadata(m_metadataTag)) {
              phi.setMetadata(m_metadataTag, meta);
              phi.setMetadata(m_memPhiTag, meta);
              break;
            }
          }
        }
      }
    }
  }

  /// \brief Put memUses (shadow loads, etc.,) into a solved form. This
  /// optimizes loads to use the most recent non-clobbering defs. Not that this
  /// is an optimization over the existing Memory SSA form.
  void solveUses(Function &F);

  // Helper functions used by `solveUses`.
  using MaybeAllocSites = Optional<SmallDenseSet<Value *, 8>>;
  using AllocSitesCache = DenseMap<Value *, MaybeAllocSites>;
  bool isMemInit(const CallInst &memOp);
  CallInst &getParentDef(CallInst &memOp);
  const MaybeAllocSites &getAllAllocSites(Value &ptr, AllocSitesCache &cache);
  bool mayClobber(CallInst &memDef, CallInst &memUse, AllocSitesCache &cache);

public:
  ShadowMemImpl(dsa::GlobalAnalysis &dsa, dsa::AllocSiteInfo &asi /*unused*/,
                TargetLibraryInfo &tli, CallGraph *cg, Pass &pass,
                bool splitDsaNodes, bool computeReadMod, bool memOptimizer,
                bool useTBAA)
      : m_dsa(dsa), m_tli(tli), m_dl(nullptr), m_callGraph(cg),
        m_pass(pass), m_splitDsaNodes(splitDsaNodes),
        m_computeReadMod(computeReadMod), m_memOptimizer(memOptimizer),
        m_useTBAA(useTBAA) {}

  bool runOnModule(Module &M) {
    m_dl = &M.getDataLayout();

    if (m_dsa.kind() != GlobalAnalysisKind::CONTEXT_INSENSITIVE) {
      // this refinement might be useful only if sea-dsa is
      // context-insensitive.
      m_computeReadMod = false;
    }

    if (m_computeReadMod)
      doReadMod();

    mkShadowFunctions(M);
    m_nodeIds.clear();

    bool changed = false;
    for (Function &F : M)
      changed |= runOnFunction(F);
    return changed;
  };

  bool runOnFunction(Function &F);
  void visitFunction(Function &F);
  void visitMainFunction(Function &F);
  void visitAllocaInst(AllocaInst &I);
  void visitLoadInst(LoadInst &I);
  void visitStoreInst(StoreInst &I);
  void visitCallSite(CallSite CS);
  void visitMemSetInst(MemSetInst &I);
  void visitMemTransferInst(MemTransferInst &I);
  void visitAllocationFn(CallSite &CS);
  void visitCalloc(CallSite &CS);
  void visitDsaCallSite(dsa::DsaCallSite &CS);


  /// \brief Returns a reference to the global sea-dsa analysis.
  GlobalAnalysis &getDsaAnalysis() {
    return m_dsa;
  }

  /// \brief Return whether or not dsa nodes are split by fields.
  bool splitDsaNodes() const {
    return m_splitDsaNodes;
  }

  /// \brief Return the id for the cell.
  /// It should be call after ShadowMem has finished. Note that this
  /// method unlike getFieldId does not allocate a new id if the cell
  /// does not have one.
  llvm::Optional<unsigned> getCellId(const Cell &c) const {
    const dsa::Node *n = c.getNode();
    assert(n);
    unsigned offset = getOffset(c);
    auto it = m_nodeIds.find(n);
    if (it != m_nodeIds.end())
      return it->second + offset;
    else
      return llvm::None;
  }
  
  /// \brief Returns the cell associated to the shadow memory
  /// instruction. If the instruction is not a shadow memory
  /// instruction then it returns null.
  llvm::Optional<Cell> getShadowMemCell(const CallInst &ci) const {
    if (!isShadowMemInst(ci)) {
      LOG("shadow_cs", errs() << "Warning: " << ci
                              << " is not a shadow memory instruction.\n";);
      return llvm::None;
    }
    auto it = m_shadowMemInstToCell.find(&ci);
    if (it != m_shadowMemInstToCell.end()) {
      return llvm::Optional<Cell>(it->second);
    } else {
      LOG("shadow_cs",
          errs() << "Warning: cannot find cell associated to shadow mem inst "
                 << ci << "\n";);
      return llvm::None;
    }
  }

  ShadowMemInstOp getShadowMemOp(const CallInst &ci) const {
    if (!isShadowMemInst(ci))
      return ShadowMemInstOp::UNKNOWN;

    ImmutableCallSite CS(&ci);
    const Function *callee = CS.getCalledFunction();
    if (!callee)
      return ShadowMemInstOp::UNKNOWN;

    if (callee->getName().equals(m_memLoadTag)) {
      return ShadowMemInstOp::LOAD;
    } else if (callee->getName().equals(m_memTrsfrLoadTag)) {
      return ShadowMemInstOp::TRSFR_LOAD;
    } else if (callee->getName().equals(m_memStoreTag)) {
      return ShadowMemInstOp::STORE;
    } else if (callee->getName().equals(m_memGlobalVarInitTag)) {
      return ShadowMemInstOp::GLOBAL_INIT;
    } else if (callee->getName().equals(m_memInitTag)) {
      return ShadowMemInstOp::INIT;
    } else if (callee->getName().equals(m_memArgInitTag)) {
      return ShadowMemInstOp::ARG_INIT;
    } else if (callee->getName().equals(m_memArgRefTag)) {
      return ShadowMemInstOp::ARG_REF;
    } else if (callee->getName().equals(m_memArgModTag)) {
      return ShadowMemInstOp::ARG_MOD;
    } else if (callee->getName().equals(m_memArgNewTag)) {
      return ShadowMemInstOp::ARG_NEW;
    } else if (callee->getName().equals(m_memFnInTag)) {
      return ShadowMemInstOp::FUN_IN;
    } else if (callee->getName().equals(m_memFnOutTag)) {
      return ShadowMemInstOp::FUN_OUT;
    } else {
      return ShadowMemInstOp::UNKNOWN;
    }
  }

  std::pair<Value *, Value *> getShadowMemVars(CallInst &ci) const {
    Value *def = nullptr;
    Value *use = nullptr;

    CallSite CS(&ci);
    switch (getShadowMemOp(ci)) {
    case ShadowMemInstOp::LOAD:
    case ShadowMemInstOp::ARG_REF:
    case ShadowMemInstOp::FUN_OUT:
    case ShadowMemInstOp::TRSFR_LOAD:
      use = CS.getArgument(1);
      break;
    case ShadowMemInstOp::STORE:
    case ShadowMemInstOp::ARG_MOD:
    case ShadowMemInstOp::ARG_NEW:
    case ShadowMemInstOp::GLOBAL_INIT:      
      use = CS.getArgument(1);
      def = &ci;
      break;
    case ShadowMemInstOp::FUN_IN:
    case ShadowMemInstOp::INIT:
    case ShadowMemInstOp::ARG_INIT:
      def = &ci;
      break;
    default:;
      ;
    }
    return {def, use};
  }

  const llvm::StringRef m_metadataTag = "shadow.mem";
  const llvm::StringRef m_memDefTag = "shadow.mem.def";
  const llvm::StringRef m_memUseTag = "shadow.mem.use";
  const llvm::StringRef m_memPhiTag = "shadow.mem.phi";

  const llvm::StringRef m_memLoadTag = "shadow.mem.load";
  const llvm::StringRef m_memTrsfrLoadTag = "shadow.mem.trsfr.load";
  const llvm::StringRef m_memStoreTag = "shadow.mem.store";
  const llvm::StringRef m_memGlobalVarInitTag = "shadow.mem.global.init";
  const llvm::StringRef m_memInitTag = "shadow.mem.init";
  const llvm::StringRef m_memArgInitTag = "shadow.mem.arg.init";
  const llvm::StringRef m_memArgRefTag = "shadow.mem.arg.ref";
  const llvm::StringRef m_memArgModTag = "shadow.mem.arg.mod";
  const llvm::StringRef m_memArgNewTag = "shadow.mem.arg.new";
  const llvm::StringRef m_memFnInTag = "shadow.mem.in";
  const llvm::StringRef m_memFnOutTag = "shadow.mem.out";
};

bool ShadowMemImpl::runOnFunction(Function &F) {
  if (F.isDeclaration())
    return false;

  if (!m_dsa.hasGraph(F))
    return false;

  m_graph = &m_dsa.getGraph(F);
  m_shadows.clear();

  // Alias analyses in LLVM are function passes, so we can only get the results
  // once the function is known. The Pass Manager is not able to schedule the
  // AA's here, construct them manually as a workaround.
  AAResults results(m_tli);
  DominatorTree &dt =
      m_pass.getAnalysis<llvm::DominatorTreeWrapperPass>(F).getDomTree();
  AssumptionCache &ac =
      m_pass.getAnalysis<llvm::AssumptionCacheTracker>().getAssumptionCache(F);
  BasicAAResult baa(*m_dl, m_tli, ac, &dt);
  // AA's need to be registered in the results object that will finish their
  // initialization.
  results.addAAResult(baa);

  std::unique_ptr<TypeBasedAAResult> tbaa = nullptr;
  if (m_useTBAA) {
    tbaa = llvm::make_unique<TypeBasedAAResult>();
    results.addAAResult(*tbaa);
  }

  m_localAAResults = LocalAAResultsWrapper(&baa, tbaa.get());

  // -- instrument function F with pseudo-instructions
  m_llvmCtx = &F.getContext();
  m_B = llvm::make_unique<IRBuilder<>>(*m_llvmCtx);

  this->visit(F);

  auto &B = *m_B;
  auto &G = *m_graph;
  // -- compute pseudo-functions for inputs and outputs

  // compute Nodes that escape because they are either reachable
  // from the input arguments or from returns
  std::set<const dsa::Node *, NodeOrdering> reach;
  std::set<const dsa::Node *, NodeOrdering> retReach;
  argReachableNodes(F, G, reach, retReach);

  // -- create shadows for all nodes that are modified by this
  // -- function and escape to a parent function
  for (const dsa::Node *n : reach)
    if (isModified(n, F) || isRead(n, F)) {
      // TODO: allocate for all slices of n, not just offset 0
      getShadowForField(*n, 0);
    }

  // allocate initial value for all used shadows
  DenseMap<const dsa::Node *, DenseMap<unsigned, Value *>> inits;
  B.SetInsertPoint(&*F.getEntryBlock().begin());
  for (const auto &nodeToShadow : m_shadows) {
    const dsa::Node *n = nodeToShadow.first;

    Constant *fn =
        reach.count(n) <= 0 ? m_memShadowInitFn : m_memShadowArgInitFn;

    for (auto jt : nodeToShadow.second) {
      dsa::Cell c(const_cast<dsa::Node *>(n), jt.first);
      AllocaInst *a = jt.second;
      inits[c.getNode()][getOffset(c)] = &mkShadowAllocInit(B, fn, a, c);
    }
  }

  ReturnInst *_ret = nullptr;
  ::HasReturn(F, _ret);
  if (!_ret) {
    // TODO: Need to think how to handle functions that do not return in
    // interprocedural encoding. For now, we print a warning and ignore this
    // case.
    errs() << "WARNING: Function " << F.getName() << "never returns."
           << "Inter-procedural analysis with such functions might not be "
           << "supported.";
    return true;
  }
  TerminatorInst *ret = cast<ReturnInst>(_ret);
  BasicBlock *exit = ret->getParent();

  // split return basic block if it has more than just the return instruction
  if (exit->size() > 1) {
    exit = llvm::SplitBlock(exit, ret,
                            // these two passes will not be preserved if null
                            nullptr /*DominatorTree*/, nullptr /*LoopInfo*/);
    ret = exit->getTerminator();
  }

  B.SetInsertPoint(ret);
  unsigned idx = 0;
  for (const dsa::Node *n : reach) {
    // TODO: extend to work with all slices
    dsa::Cell c(const_cast<dsa::Node *>(n), 0);

    // n is read and is not only return-node reachable (for
    // return-only reachable nodes, there is no initial value
    // because they are created within this function)
    if ((isRead(n, F) || isModified(n, F)) && retReach.count(n) <= 0) {
      assert(!inits[n].empty());
      /// initial value
      mkMarkIn(B, c, inits[n][0], idx, llvm::None);
    }

    if (isModified(n, F)) {
      assert(!inits[n].empty());
      /// final value
      mkMarkOut(B, c, idx, llvm::None);
    }
    ++idx;
  }

  // -- convert new allocas to registers
  runMem2Reg(F);
  // -- infer types for PHINodes
  inferTypeOfPHINodes(F);

  if (m_memOptimizer)
    solveUses(F);

  m_B.reset(nullptr);
  m_llvmCtx = nullptr;
  m_graph = nullptr;
  m_shadows.clear();
  m_shadowOpToAccessedPtr.clear();
  m_shadowOpToConcreteInst.clear();

  return true;
}

void ShadowMemImpl::visitFunction(Function &fn) {
  if (fn.getName().equals("main")) {
    visitMainFunction(fn);
  }
}

void ShadowMemImpl::visitMainFunction(Function &fn) {
  // set insertion point to the beginning of the main function
  m_B->SetInsertPoint(&*fn.getEntryBlock().begin());

  std::set<GlobalVariable *, GlobalVariableOrdering> globals;
  for (auto &gv : fn.getParent()->globals()) {
    globals.insert(&gv);
  }

  // iterate over all globals
  for (auto gv : globals) {
    // skip globals that are used internally by llvm
    if (gv->getSection().equals("llvm.metadata"))
      continue;
    // skip globals that do not appear in alias analysis
    if (!m_graph->hasCell(*gv))
      continue;
    // insert call to mkShadowGlobalVarInit()
    auto sz = dsa::getTypeSizeInBytes(*(gv->getValueType()), *m_dl);
    if (CallInst *memInit =
            mkShadowGlobalVarInit(*m_B, m_graph->getCell(*gv), *gv, sz))
      associateConcretePtr(*memInit, *gv, nullptr);
  }
}

void ShadowMemImpl::visitAllocaInst(AllocaInst &I) {
  if (!m_graph->hasCell(I))
    return;

  const dsa::Cell &c = m_graph->getCell(I);
  if (c.isNull())
    return;

  assert(dsa::AllocSiteInfo::isAllocSite(I));

  m_B->SetInsertPoint(&I);
  CallInst &memUse =
      mkShadowLoad(*m_B, c, dsa::AllocSiteInfo::getAllocSiteSize(I));
  associateConcretePtr(memUse, I, &I);
}

void ShadowMemImpl::visitLoadInst(LoadInst &I) {
  Value *loadSrc = I.getPointerOperand();

  if (!m_graph->hasCell(*loadSrc))
    return;
  const dsa::Cell &c = m_graph->getCell(*loadSrc);
  if (c.isNull())
    return;

  m_B->SetInsertPoint(&I);
  CallInst &memUse =
      mkShadowLoad(*m_B, c, dsa::getTypeSizeInBytes(*I.getType(), *m_dl));
  associateConcretePtr(memUse, *loadSrc, &I);
}

void ShadowMemImpl::visitStoreInst(StoreInst &I) {
  Value *storeDst = I.getPointerOperand();
  if (!m_graph->hasCell(*storeDst))
    return;
  const dsa::Cell &c = m_graph->getCell(*storeDst);
  if (c.isNull())
    return;

  m_B->SetInsertPoint(&I);
  CallInst &memDef = mkShadowStore(
      *m_B, c, dsa::getTypeSizeInBytes(*I.getValueOperand()->getType(), *m_dl));
  associateConcretePtr(memDef, *storeDst, &I);
}

void ShadowMemImpl::visitCallSite(CallSite CS) {
  if (CS.isIndirectCall())
    return;

  auto *callee = CS.getCalledFunction();
  if (!callee)
    return;

  if ((callee->getName().startswith("seahorn.") ||
       callee->getName().startswith("verifier.")) &&
      /* seahorn.bounce should be treated as a regular function*/
      !(callee->getName().startswith("seahorn.bounce")))
    return;

  auto *callInst = CS.getInstruction();

  LOG("shadow_cs", errs() << "Call: " << *callInst << "\n";);

  if (callee->getName().equals("calloc")) {
    visitCalloc(CS);
    return;
  }

  if (dsa::AllocSiteInfo::isAllocSite(*callInst) &&
      /* we don't want to treat specially allocation wrappers */
      (callee->isDeclaration() || callee->empty())) {
    visitAllocationFn(CS);
    return;
  }

  if (m_dsa.hasGraph(*callee)) {
    ImmutableCallSite ics(callInst);
    dsa::DsaCallSite dcs(ics);
    visitDsaCallSite(dcs);
    return;
  }
}

void ShadowMemImpl::visitDsaCallSite(dsa::DsaCallSite &CS) {
  const Function &CF = *CS.getCallee();

  dsa::Graph &calleeG = m_dsa.getGraph(CF);

  // -- compute callee nodes reachable from arguments and returns
  std::set<const dsa::Node *, NodeOrdering> reach;
  std::set<const dsa::Node *, NodeOrdering> retReach;
  argReachableNodes(CF, calleeG, reach, retReach);

  // -- compute mapping between callee and caller graphs
  dsa::SimulationMapper simMap;
  dsa::Graph::computeCalleeCallerMapping(CS, calleeG, *m_graph, simMap);

  /// generate mod, ref, new function, based on whether the
  /// remote node reads, writes, or creates the corresponding node.

  m_B->SetInsertPoint(const_cast<Instruction *>(CS.getInstruction()));
  unsigned idx = 0;
  for (const dsa::Node *n : reach) {
    LOG("global_shadow", errs() << *n << "\n";
        const Value *v = n->getUniqueScalar();
        if (v) errs() << "value: " << *n->getUniqueScalar() << "\n";
        else errs() << "no unique scalar\n";);

    // skip nodes that are not read/written by the callee
    if (!isRead(n, CF) && !isModified(n, CF))
      continue;

    // TODO: This must be done for every possible offset of the caller
    // node,
    // TODO: not just for offset 0

    assert(n);
    dsa::Cell callerC = simMap.get(dsa::Cell(const_cast<dsa::Node *>(n), 0));
    assert(!callerC.isNull() && "Not found node in the simulation map");

    // allocate shadow for callerC
    getShadowForField(callerC);
    // assign an id to callerC
    getFieldId(callerC);

    // -- read only node ignore nodes that are only reachable
    // -- from the return of the function
    if (isRead(n, CF) && !isModified(n, CF) && retReach.count(n) <= 0) {
      mkArgRef(*m_B, callerC, idx, llvm::None);
      // Unclear how to get the associated concrete pointer here.
    }
    // -- read/write or new node
    else if (isModified(n, CF)) {
      // -- n is new node iff it is reachable only from the return node
      Constant *argFn = retReach.count(n) ? m_argNewFn : m_argModFn;
      mkArgNewMod(*m_B, argFn, callerC, idx, llvm::None);
      // Unclear how to get the associated concrete pointer here.
    }
    idx++;
  }
}

void ShadowMemImpl::visitCalloc(CallSite &CS) {
  if (!m_graph->hasCell(*CS.getInstruction()))
    return;
  const dsa::Cell &c = m_graph->getCell(*CS.getInstruction());
  if (c.isNull())
    return;

  auto &callInst = *CS.getInstruction();
  assert(dsa::AllocSiteInfo::isAllocSite(callInst));

  if (c.getOffset() == 0) {
    m_B->SetInsertPoint(&callInst);
    CallInst &memDef =
        mkShadowStore(*m_B, c, dsa::AllocSiteInfo::getAllocSiteSize(callInst));
    associateConcretePtr(memDef, callInst, &callInst);
  } else {
    // TODO: handle multiple nodes
    errs() << "WARNING: skipping calloc instrumentation because cell "
           << "offset is not zero\n";
  }
}

void ShadowMemImpl::visitAllocationFn(CallSite &CS) {
  if (!m_graph->hasCell(*CS.getInstruction()))
    return;
  const dsa::Cell &c = m_graph->getCell(*CS.getInstruction());
  if (c.isNull())
    return;

  auto &callInst = *CS.getInstruction();
  m_B->SetInsertPoint(&callInst);

  CallInst &memUse =
      mkShadowLoad(*m_B, c, dsa::AllocSiteInfo::getAllocSiteSize(callInst));
  assert(dsa::AllocSiteInfo::isAllocSite(callInst));
  associateConcretePtr(memUse, callInst, &callInst);
}

void ShadowMemImpl::visitMemSetInst(MemSetInst &I) {
  LOG("dsa.memset", errs() << "Visitng memset: " << I << "\n";);
  Value &dst = *(I.getDest());

  if (!m_graph->hasCell(dst)) {
    LOG("dsa.memset", errs() << "No DSA cell for " << dst << "\n";);
    return;
  }
  const dsa::Cell &c = m_graph->getCell(dst);
  if (c.isNull()) {
    LOG("dsa.memset", errs() << "Dst cell is null\n";);
    return;
  }

  // XXX: Don't remember why this check was needed. Disabling for now.
  // if (c.getOffset() != 0) return;
  m_B->SetInsertPoint(&I);

  Optional<unsigned> len = llvm::None;
  if (auto *sz = dyn_cast_or_null<ConstantInt>(I.getLength()))
    len = sz->getLimitedValue();

  CallInst &memDef = mkShadowStore(*m_B, c, len);
  associateConcretePtr(memDef, dst, &I);
}

void ShadowMemImpl::visitMemTransferInst(MemTransferInst &I) {
  Value &dst = *(I.getDest());
  Value &src = *(I.getSource());

  if (!m_graph->hasCell(dst))
    return;
  if (!m_graph->hasCell(src))
    return;
  const dsa::Cell &dstC = m_graph->getCell(dst);
  const dsa::Cell &srcC = m_graph->getCell(src);
  if (dstC.isNull())
    return;

  /*
  // XXX don't remember why this check is needed
  if (dstC.getOffset() != 0)
    return;
  */
  Optional<unsigned> len = llvm::None;
  if (auto *length = dyn_cast_or_null<ConstantInt>(I.getLength()))
    len = length->getLimitedValue();

  m_B->SetInsertPoint(&I);
  auto useDefPair = mkShadowMemTrsfr(*m_B, dstC, srcC, len);
  associateConcretePtr(useDefPair.first, src, &I);
  associateConcretePtr(useDefPair.second, dst, &I);
}

void ShadowMemImpl::mkShadowFunctions(Module &M) {
  LLVMContext &ctx = M.getContext();
  m_Int32Ty = Type::getInt32Ty(ctx);
  Type *i8PtrTy = Type::getInt8PtrTy(ctx);
  Type *voidTy = Type::getVoidTy(ctx);

  m_memLoadFn = M.getOrInsertFunction(m_memLoadTag, voidTy, m_Int32Ty,
                                      m_Int32Ty, i8PtrTy);

  m_memTrsfrLoadFn = M.getOrInsertFunction(m_memTrsfrLoadTag, voidTy, m_Int32Ty,
                                           m_Int32Ty, i8PtrTy);

  m_memStoreFn = M.getOrInsertFunction(m_memStoreTag, m_Int32Ty, m_Int32Ty,
                                       m_Int32Ty, i8PtrTy);

  m_memGlobalVarInitFn = M.getOrInsertFunction(m_memGlobalVarInitTag, m_Int32Ty,
                                               m_Int32Ty, m_Int32Ty, i8PtrTy);

  m_memShadowInitFn =
      M.getOrInsertFunction(m_memInitTag, m_Int32Ty, m_Int32Ty, i8PtrTy);

  m_memShadowArgInitFn =
      M.getOrInsertFunction(m_memArgInitTag, m_Int32Ty, m_Int32Ty, i8PtrTy);

  m_argRefFn = M.getOrInsertFunction(m_memArgRefTag, voidTy, m_Int32Ty,
                                     m_Int32Ty, m_Int32Ty, i8PtrTy);

  m_argModFn = M.getOrInsertFunction(m_memArgModTag, m_Int32Ty, m_Int32Ty,
                                     m_Int32Ty, m_Int32Ty, i8PtrTy);

  m_argNewFn = M.getOrInsertFunction(m_memArgNewTag, m_Int32Ty, m_Int32Ty,
                                     m_Int32Ty, m_Int32Ty, i8PtrTy);

  m_markIn = M.getOrInsertFunction(m_memFnInTag, voidTy, m_Int32Ty, m_Int32Ty,
                                   m_Int32Ty, i8PtrTy);
  m_markOut = M.getOrInsertFunction(m_memFnOutTag, voidTy, m_Int32Ty, m_Int32Ty,
                                    m_Int32Ty, i8PtrTy);

  m_memInitFunctions = {m_memShadowInitFn, m_memGlobalVarInitFn,
                        m_memShadowArgInitFn};
  // m_memInitFunctions = {m_memShadowInitFn, m_memGlobalVarInitFn,
  //                       m_memShadowArgInitFn, m_memShadowUniqArgInitFn,
  //                       m_memShadowUniqInitFn};
}

void ShadowMemImpl::doReadMod() {
  if (!m_callGraph) {
    errs() << "WARNING: Call graph is missing. Not computing local mod/ref.";
    return;
  }

  // for every scc in bottom-up order
  for (auto it = scc_begin(m_callGraph); !it.isAtEnd(); ++it) {
    NodeSet read;
    const std::vector<CallGraphNode *> &scc = *it;
    NodeSet modified;

    // compute read/mod, sharing information between scc
    for (CallGraphNode *cgn : scc) {
      Function *f = cgn->getFunction();
      if (!f)
        continue;
      updateReadMod(*f, read, modified);
    }

    // set the computed read/mod to all functions in the scc
    for (CallGraphNode *cgn : scc) {
      Function *f = cgn->getFunction();
      if (!f)
        continue;
      m_readList[f].insert(read.begin(), read.end());
      m_modList[f].insert(modified.begin(), modified.end());
    }
  }
}

void ShadowMemImpl::updateReadMod(Function &F, NodeSet &readSet,
                                  NodeSet &modSet) {
  if (!m_dsa.hasGraph(F))
    return;

  dsa::Graph &G = m_dsa.getGraph(F);
  for (Instruction &inst : llvm::instructions(F)) {
    if (auto *li = dyn_cast<LoadInst>(&inst)) {
      if (G.hasCell(*(li->getPointerOperand()))) {
        const dsa::Cell &c = G.getCell(*(li->getPointerOperand()));
        if (!c.isNull())
          readSet.insert(c.getNode());
      }
    } else if (auto *si = dyn_cast<StoreInst>(&inst)) {
      if (G.hasCell(*(si->getPointerOperand()))) {
        const dsa::Cell &c = G.getCell(*(si->getPointerOperand()));
        if (!c.isNull())
          modSet.insert(c.getNode());
      }
    } else if (auto *ci = dyn_cast<CallInst>(&inst)) {
      CallSite CS(ci);
      Function *cf = CS.getCalledFunction();

      if (!cf)
        continue;
      if (cf->getName().equals("calloc")) {
        const dsa::Cell &c = G.getCell(inst);
        if (!c.isNull())
          modSet.insert(c.getNode());
      } else if (m_dsa.hasGraph(*cf)) {
        assert(&(m_dsa.getGraph(*cf)) == &G &&
               "Computing local mod/ref in context sensitive mode");
        readSet.insert(m_readList[cf].begin(), m_readList[cf].end());
        modSet.insert(m_modList[cf].begin(), m_modList[cf].end());
      }
    }
    // TODO: handle intrinsics (memset,memcpy) and other library functions
  }
}

void ShadowMemImpl::associateConcretePtr(CallInst &memOp, Value &ptr,
                                         Value *inst) {
  assert(memOp.getMetadata(m_metadataTag));
  if (auto *i = dyn_cast<Instruction>(&ptr))
    assert(!i->getMetadata(m_metadataTag));
  if (auto *i = dyn_cast_or_null<Instruction>(inst))
    assert(!i->getMetadata(m_metadataTag));

  m_shadowOpToAccessedPtr.insert({&memOp, &ptr});
  if (inst)
    m_shadowOpToConcreteInst.insert({&memOp, inst});
}

Value *ShadowMemImpl::getAssociatedConcretePtr(CallInst &memOp) {
  assert(memOp.getMetadata(m_metadataTag));
  auto it = m_shadowOpToAccessedPtr.find(&memOp);
  if (it != m_shadowOpToAccessedPtr.end())
    return it->second;

  return nullptr;
}

Value *ShadowMemImpl::getAssociatedConcreteInst(CallInst &memOp) {
  assert(memOp.getMetadata(m_metadataTag));
  auto it = m_shadowOpToConcreteInst.find(&memOp);
  if (it != m_shadowOpToConcreteInst.end())
    return it->second;

  return nullptr;
}

bool ShadowMemImpl::isMemInit(const CallInst &memOp) {
  assert(memOp.getMetadata(m_metadataTag));
  auto *fn = memOp.getCalledFunction();
  assert(fn);
  return llvm::is_contained(m_memInitFunctions, fn);
}

CallInst &ShadowMemImpl::getParentDef(CallInst &memOp) {
  assert(memOp.getMetadata(m_metadataTag));

  if (isMemInit(memOp))
    return memOp;

  if (auto *fn = memOp.getCalledFunction()) {
    assert(fn == m_memLoadFn || fn == m_memStoreFn || fn == m_memTrsfrLoadFn);
  } else {
    assert(false && "CallInst should be a shadow mem call");
  }

  assert(memOp.getNumOperands() >= 1);
  Value *defArg = memOp.getOperand(1);
  assert(defArg);
  assert(isa<CallInst>(defArg));
  auto *def = cast<CallInst>(defArg);
  assert(def->getMetadata(m_memDefTag));

  return *def;
}

const ShadowMemImpl::MaybeAllocSites &
ShadowMemImpl::getAllAllocSites(Value &ptr, AllocSitesCache &cache) {
  // Performs a simple walk over use-def chains until it finds all allocation
  // sites or an instruction it cannot look thru (e.g., a load).
  assert(ptr.getType()->isPointerTy());

  auto *strippedInit = ptr.stripPointerCastsAndBarriers();
  {
    auto it = cache.find(strippedInit);
    if (it != cache.end())
      return it->second;
  }

  SmallDenseSet<Value *, 8> allocSites;
  DenseSet<Value *> visited;
  SmallVector<Value *, 8> worklist = {strippedInit};

  while (!worklist.empty()) {
    Value *current = worklist.pop_back_val();
    assert(current);
    Value *stripped = current->stripPointerCastsAndBarriers();
    if (visited.count(stripped) > 0)
      continue;

    visited.insert(stripped);
    LOG("shadow_optimizer", llvm::errs() << "Visiting: " << *stripped << "\n");

    if (isa<AllocaInst>(stripped) || isa<Argument>(stripped) ||
        isa<GlobalValue>(stripped)) {
      allocSites.insert(stripped);
      continue;
    }

    // Check with the DSA if a call is considered an allocation site.
    if (auto *ci = dyn_cast<CallInst>(stripped))
      if (m_graph->hasAllocSiteForValue(*ci)) {
        allocSites.insert(stripped);
        continue;
      }

    if (auto *gep = dyn_cast<GetElementPtrInst>(stripped)) {
      worklist.push_back(gep->getPointerOperand());
      continue;
    }

    if (auto *phi = dyn_cast<PHINode>(stripped)) {
      for (Value *v : llvm::reverse(phi->incoming_values()))
        worklist.push_back(v);

      continue;
    }

    if (auto *gamma = dyn_cast<SelectInst>(stripped)) {
      worklist.push_back(gamma->getFalseValue());
      worklist.push_back(gamma->getTrueValue());
      continue;
    }

    LOG("shadow_optimizer",
        llvm::errs() << "Cannot retrieve all alloc sites for " << ptr
                     << "\n\tbecause of the instruction: " << *stripped
                     << "\n");
    return (cache[strippedInit] = llvm::None);
  }

  return (cache[strippedInit] = allocSites);
}

bool ShadowMemImpl::mayClobber(CallInst &memDef, CallInst &memUse,
                               AllocSitesCache &cache) {
  LOG("shadow_optimizer", llvm::errs() << "\n~~~~\nmayClobber(" << memDef
                                       << ", " << memUse << ")?\n");

  if (isMemInit(memDef))
    return true;

  LOG("shadow_optimizer", llvm::errs() << "\tmayClobber[1]\n");

  // Get the corresponding pointers and check if these two pointers have
  // disjoint allocation sites -- if not, they cannot alias.
  // This can be extended further with offset tracking.

  Value *usePtr = getAssociatedConcretePtr(memUse);
  if (!usePtr)
    return true;

  LOG("shadow_optimizer", llvm::errs() << "\tmayClobber[2 ]\n");

  Value *defPtr = getAssociatedConcretePtr(memDef);
  if (!defPtr)
    return true;

  usePtr = usePtr->stripPointerCastsAndBarriers();
  defPtr = defPtr->stripPointerCastsAndBarriers();
  assert(m_graph->hasCell(*usePtr));
  assert(m_graph->hasCell(*defPtr));

  LOG("shadow_optimizer",
      llvm::errs() << "alias(" << *usePtr << ", " << *defPtr << ")?\n");

  const Optional<unsigned> usedBytes =
      maybeGetMetaConstant(memUse, m_memUseTag);
  const Optional<unsigned> defdBytes =
      maybeGetMetaConstant(memDef, m_memDefTag);

  // If the offsets don't overlap, no clobbering may happen.
  {
    const dsa::Cell &useCell = m_graph->getCell(*usePtr);
    const dsa::Cell &defCell = m_graph->getCell(*defPtr);
    assert(useCell.getNode() == defCell.getNode());
    // This works with arrays in SeaDsa because all array elements share the
    // same abstract field.

    const unsigned useStartOffset = useCell.getOffset();
    const unsigned defStartOffset = defCell.getOffset();
    LOG("shadow_optimizer",
        llvm::errs() << "\tmayClobber[3]\n\tuseStart: " << useStartOffset
                     << "\n\tdefStart: " << defStartOffset << "\n");

    if (defdBytes.hasValue())
      if (useStartOffset >= defStartOffset + *defdBytes)
        return false;

    if (usedBytes.hasValue())
      if (useStartOffset + *usedBytes <= defStartOffset)
        return false;
  }
  LOG("shadow_optimizer", llvm::errs() << "\tmayClobber[4]\n");

  {
    Value *concreteInstUse = getAssociatedConcreteInst(memUse);
    Value *concreteInstDef = getAssociatedConcreteInst(memDef);
    if (m_localAAResults.isNoAlias(*usePtr, concreteInstUse, usedBytes, *defPtr,
                                   concreteInstDef, defdBytes))
      return false;
  }

  LOG("shadow_optimizer", llvm::errs() << "\tmayClobber[6]\n");

  auto useAllocSites = getAllAllocSites(*usePtr, cache);
  if (!useAllocSites.hasValue())
    return true;

  LOG("shadow_optimizer", llvm::errs() << "\tmayClobber[7]\n");

  auto defAllocSites = getAllAllocSites(*defPtr, cache);
  if (!defAllocSites.hasValue())
    return true;

  LOG("shadow_optimizer", llvm::errs() << "\tmayClobber[8]\n");

  for (auto *as : *useAllocSites)
    if (defAllocSites->count(as) > 0)
      return true;

  LOG("shadow_optimizer", llvm::errs() << "\tmayClobber[9]\n");

  return false;
}

void ShadowMemImpl::solveUses(Function &F) {
  unsigned numOptimized = 0;

  AllocSitesCache ptrToAllocSitesCache;

  // Visit shadow loads in topological order.
  for (auto *bb : llvm::inverse_post_order(&F.getEntryBlock()))
    for (auto &inst : *bb)
      if (auto *call = dyn_cast<CallInst>(&inst))
        if (call->getMetadata(m_memUseTag)) {
          LOG("shadow_optimizer", llvm::errs() << "Solving " << *call << "\n");

          // Find the first potentially clobbering memDef (shadow store or
          // shadow init).
          CallInst *oldDef = nullptr;
          CallInst *def = &getParentDef(*call);
          while (oldDef != def &&
                 !mayClobber(*def, *call, ptrToAllocSitesCache)) {
            oldDef = def;
            def = &getParentDef(*def);
          }

          LOG("shadow_optimizer", llvm::errs() << "Def for: " << *call
                                               << "\n\tis " << *def << "\n");

          if (call->getOperand(1) != def) {
            ++numOptimized;
            call->setArgOperand(1, def);
          }
        }

  LOG("shadow_optimizer", llvm::errs() << "MemSSA optimizer: " << numOptimized
                                       << " use(s) solved.\n");
}

/** ShadowMem class **/
ShadowMem::ShadowMem(GlobalAnalysis &dsa, AllocSiteInfo &asi,
                     llvm::TargetLibraryInfo &tli, llvm::CallGraph *cg,
                     llvm::Pass &pass, bool splitDsaNodes, bool computeReadMod,
                     bool memOptimizer, bool useTBAA)
    : m_impl(new ShadowMemImpl(dsa, asi, tli, cg, pass, splitDsaNodes,
                               computeReadMod, memOptimizer, useTBAA)) {}

bool ShadowMem::runOnModule(Module &M) { return m_impl->runOnModule(M); }

GlobalAnalysis &ShadowMem::getDsaAnalysis() {
  return m_impl->getDsaAnalysis();
}

bool ShadowMem::splitDsaNodes() const {
  return m_impl->splitDsaNodes();
}
  
llvm::Optional<unsigned> ShadowMem::getCellId(const dsa::Cell &c) const {
  return m_impl->getCellId(c);
}

ShadowMemInstOp ShadowMem::getShadowMemOp(const CallInst &ci) const {
  return m_impl->getShadowMemOp(ci);
}

llvm::Optional<Cell> ShadowMem::getShadowMemCell(const CallInst &ci) const {
  return m_impl->getShadowMemCell(ci);
}

std::pair<Value *, Value *> ShadowMem::getShadowMemVars(CallInst &ci) const {
  return m_impl->getShadowMemVars(ci);
}

/** ShadowMemPass class **/
ShadowMemPass::ShadowMemPass()
    : llvm::ModulePass(ShadowMemPass::ID), m_shadowMem(nullptr) {}

bool ShadowMemPass::runOnModule(llvm::Module &M) {
  if (M.begin() == M.end())
    return false;

  GlobalAnalysis &dsa = getAnalysis<DsaAnalysis>().getDsaAnalysis();
  AllocSiteInfo &asi = getAnalysis<AllocSiteInfo>();
  TargetLibraryInfo &tli = getAnalysis<TargetLibraryInfoWrapperPass>().getTLI();
  CallGraph *cg = nullptr;
  auto cgPass = getAnalysisIfAvailable<CallGraphWrapperPass>();
  if (cgPass)
    cg = &cgPass->getCallGraph();

  LOG("shadow_verbose", errs() << "Module before shadow insertion:\n"
                               << M << "\n";);

  m_shadowMem.reset(new ShadowMem(dsa, asi, tli, cg, *this, SplitFields,
                                  LocalReadMod, ShadowMemOptimize,
                                  ShadowMemUseTBAA));

  bool res = m_shadowMem->runOnModule(M);
  LOG("shadow_verbose", errs() << "Module after shadow insertion:\n"
                               << M << "\n";);

  // -- verifyModule returns true if module is broken
  assert(!llvm::verifyModule(M, &errs()));
  return res;
}

const ShadowMem &ShadowMemPass::getShadowMem() const {
  assert(m_shadowMem);
  return *m_shadowMem;
}

ShadowMem &ShadowMemPass::getShadowMem() {
  assert(m_shadowMem);
  return *m_shadowMem;
}

void ShadowMemPass::getAnalysisUsage(llvm::AnalysisUsage &AU) const {
  AU.addRequiredTransitive<DsaAnalysis>();
  AU.addRequired<AllocSiteInfo>();
  AU.addRequired<llvm::CallGraphWrapperPass>();
  AU.addRequired<llvm::DominatorTreeWrapperPass>();
  AU.addRequired<llvm::TargetLibraryInfoWrapperPass>();
  AU.addRequired<llvm::AssumptionCacheTracker>();
  AU.setPreservesAll();
}

/** StripShadowMemPass class **/
void StripShadowMemPass::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesAll();
}

bool StripShadowMemPass::runOnModule(Module &M) {
  std::vector<std::string> voidFnNames = {"shadow.mem.load",
                                          "shadow.mem.arg.ref", "shadow.mem.in",
                                          "shadow.mem.out"};

  for (auto &name : voidFnNames) {
    Function *fn = M.getFunction(name);
    if (!fn)
      continue;

    while (!fn->use_empty()) {
      CallInst *ci = cast<CallInst>(fn->user_back());
      Value *last = ci->getArgOperand(ci->getNumArgOperands() - 1);
      ci->eraseFromParent();
      ::recursivelyDeleteTriviallyDeadInstructions(last);
    }
  }

  std::vector<std::string> intFnNames = {
      "shadow.mem.store", "shadow.mem.init", "shadow.mem.arg.init",
      "shadow.mem.global.init", "shadow.mem.arg.mod"};
  Value *zero = ConstantInt::get(Type::getInt32Ty(M.getContext()), 0);

  for (auto &name : intFnNames) {
    Function *fn = M.getFunction(name);
    if (!fn)
      continue;

    while (!fn->use_empty()) {
      CallInst *ci = cast<CallInst>(fn->user_back());
      Value *last = ci->getArgOperand(ci->getNumArgOperands() - 1);
      ci->replaceAllUsesWith(zero);
      ci->eraseFromParent();
      ::recursivelyDeleteTriviallyDeadInstructions(last);
    }
  }

  return true;
}

Pass *createStripShadowMemPass() { return new StripShadowMemPass(); }
Pass *createShadowMemPass() { return new ShadowMemPass(); }

char ShadowMemPass::ID = 0;
char StripShadowMemPass::ID = 0;

} // namespace sea_dsa

static llvm::RegisterPass<sea_dsa::ShadowMemPass>
    X("shadow-sea-dsa", "Add shadow.mem pseudo-functions");

static llvm::RegisterPass<sea_dsa::StripShadowMemPass>
    Y("strip-shadow-sea-dsa", "Remove shadow.mem pseudo-functions");
