#include "sea_dsa/Cloner.hh"
#include "sea_dsa/CallSite.hh"
#include "sea_dsa/support/Debug.h"
#include "llvm/IR/Instructions.h"
using namespace sea_dsa;

static llvm::cl::opt<bool> NoAllocSiteOpt(
    "sea-dsa-no-single-as-opt",
    llvm::cl::desc("Disable node splitting optimization in cloner"),
    llvm::cl::init(false), llvm::cl::Hidden);

/// Return true if the instruction allocates memory on the stack
static bool isStackAllocation(const llvm::Value *v) {
  // XXX Check whether there are other ways to allocate stack, like
  // insertvalue/extractvalue
  return llvm::isa<llvm::AllocaInst>(v);
}

/// Return true if the value is a constant with no pointers.
static bool isConstantNoPtr(const llvm::Value *v) {
  assert(v);
  if (!llvm::isa<llvm::Constant>(v))
    return false;

  // Cheaply check if the global value has a string-like name.
  if (v->hasName() && v->getName().startswith(".str."))
    return true;

  if (!v->getType()->isPointerTy())
    return false;

  auto *type = v->getType()->getPointerElementType();
  if (type->isIntegerTy() || type->isFloatingPointTy())
    return true;

  auto *compositeTy = llvm::dyn_cast<llvm::CompositeType>(type);
  if (!compositeTy)
    return false;

  return std::all_of(compositeTy->subtype_begin(), compositeTy->subtype_end(),
                     [](const llvm::Type *ty) {
                       return ty->isIntegerTy() || ty->isFloatingPointTy();
                     });
}

Node &Cloner::clone(const Node &n, bool forceAddAlloca,
                    const llvm::Value *onlyAllocSite) {
  // -- don't clone nodes that are already in the graph
  if (n.getGraph() == &m_graph)
    return *const_cast<Node *>(&n);

  if (NoAllocSiteOpt)
    onlyAllocSite = nullptr;

  const bool skipAllocas = m_strip_allocas && !forceAddAlloca;

  CachingLevel currentLevel = CachingLevel::Full;
  if (onlyAllocSite)
    currentLevel = CachingLevel::SingleAllocSite;
  else if (skipAllocas)
    currentLevel = CachingLevel::NoAllocas;

  if (currentLevel != SingleAllocSite) {
    auto it = m_deferredUnify.find(&n);
    if (it != m_deferredUnify.end()) {
      // We found a backedge and need to merge all these node together.
      const auto &nodeSet = it->second;
      assert(!nodeSet.empty());

      Node *first = *nodeSet.begin();
      for (Node *split : nodeSet)
        if (split != first) {
          split->getNode()->unify(*first->getNode());
          first = first->getNode();
        }

      m_deferredUnify.erase(it);
      const CachingLevel level =
          skipAllocas ? CachingLevel::NoAllocas : CachingLevel::Full;
      m_map.insert({&n, {first->getNode(), level}});
      first = first->getNode();
      // Copy other allocation sites -- deferred nodes may not know about all
      // of them.
      copyAllocationSites(n, *first, forceAddAlloca);

      return *first->getNode();
    }
  }

  // check the cache
  auto it = m_map.find(&n);
  if (it != m_map.end()) {
    Node &nNode = *(it->second.first->getNode());
    const CachingLevel cachedLevel = it->second.second;

    // If cloning was done in a more restricted case before,
    // then ensure that all allocas of n are now copied over to nNode.
    // It may happen that the same node is being cloned multiple times, each
    // with a different onlyAllocSite.
    if (currentLevel > cachedLevel)
      copyAllocationSites(n, nNode, forceAddAlloca, onlyAllocSite);

    // Remember that we updated the cache and potentially added more allocation
    // sites.
    it->second.second = currentLevel;
    return nNode;
  }

  // -- clone the node (except for the links)
  Node &nNode = m_graph.cloneNode(n, false /* cpAllocSites */);

  // used by bottom-up/top-down analysis: ignored by the rest of
  // analyses.
  nNode.setForeign(true);

  copyAllocationSites(n, nNode, forceAddAlloca, onlyAllocSite);

  // -- update cache
  // Don't cache single alloc sites -- they will be initially split and cloned
  // into separate nodes.
  if (currentLevel != SingleAllocSite)
    m_map.insert({&n, {&nNode, currentLevel}});
  else {
    // It may be necessary to unify this newly created node with another
    // node if they both originated from spliting the same original node.
    // Unification is necessary when we find an incoming edge to the original
    // node.
    m_deferredUnify[&n].insert(&nNode);
  }

  // Determine if we have to copy recursively if the exact allocation site
  // object is known.
  if (onlyAllocSite && isConstantNoPtr(onlyAllocSite))
    return nNode;

  // -- recursively clone reachable nodes
  for (auto &kv : n.links()) {
    // dummy link (should not happen)
    if (kv.second->isNull())
      continue;

    const unsigned rawOffset = kv.second->getRawOffset();
    // TODO: When we are running with a single allocation site, we can check
    //       if it can possibly point to somewhere at the current offset and
    //       avoid some cloning.

    // -- resolve any potential forwarding
    kv.second->getNode();
    // recursively clone the node pointed by the link
    Cell nCell(&clone(*kv.second->getNode()), rawOffset);
    // create new link
    nNode.setLink(kv.first, nCell);
  }

  // nNode can be forwarding if the original node was collapsed and the new one
  // was initially split into multiple onlyAllocSites.
  return *nNode.getNode();
}

void Cloner::copyAllocationSites(
    const Node &from, Node &to, bool forceAddAlloca,
    const llvm::Value *onlyAllocSite /* = nullptr */) {
  // -- copy allocation sites, except stack based, unless requested
  for (const llvm::Value *as : from.getAllocSites()) {
    assert(as);
    if (isStackAllocation(as) && !forceAddAlloca && m_strip_allocas)
      continue;

    if (onlyAllocSite && as != onlyAllocSite)
      continue;

    if (to.hasAllocSite(*as))
      continue;

    DsaAllocSite *site = m_graph.mkAllocSite(*as);
    assert(site);
    to.addAllocSite(*site);

    // -- update call paths on cloned allocation site
    importCallPaths(*site, to.getGraph()->getAllocSite(*as));
  }
}

void Cloner::importCallPaths(DsaAllocSite &site,
                             llvm::Optional<DsaAllocSite *> other) {
  if (isUnset())
    return;

  assert(other.hasValue());
  if (!other.hasValue())
    return;

  site.importCallPaths(*other.getValue(),
                       DsaCallSite(*m_context.m_cs.getValue()), isBottomUp());
}
