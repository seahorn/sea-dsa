#include "sea_dsa/Cloner.hh"
#include "sea_dsa/CallSite.hh"
#include "sea_dsa/support/Debug.h"
#include "llvm/IR/Instructions.h"
using namespace sea_dsa;

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

  CachingLevel currentLevel = CachingLevel::Full;
  if (onlyAllocSite)
    currentLevel = CachingLevel::SingleAlloca;
  else if (m_strip_allocas && forceAddAlloca)
    currentLevel = CachingLevel::NoAllocas;

  // check the cache
  auto it = m_map.find(&n);
  if (it != m_map.end()) {
    Node &nNode = *(it->second.first);
    const CachingLevel cachedLevel = it->second.second;

    // if cloning was done in a more restricted case before,
    // then ensure that all allocas of n are now copied over to nNode
    if (currentLevel > cachedLevel &&
        nNode.getAllocSites().size() < n.getAllocSites().size()) {
      for (const llvm::Value *as : n.getAllocSites()) {
        if (onlyAllocSite && as != onlyAllocSite)
          continue;

        if (m_strip_allocas && !forceAddAlloca && isStackAllocation(as))
          continue;

        DsaAllocSite *site = m_graph.mkAllocSite(*as);
        assert(site);
        nNode.addAllocSite(*site);

        // -- update call paths on the cloned allocation site
        importCallPaths(*site, n.getGraph()->getAllocSite(*as));
      }
    }

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
  
  // -- copy allocation sites, except stack based, unless requested
  for (const llvm::Value *as : n.getAllocSites()) {
    if (isStackAllocation(as)) {
      if (!forceAddAlloca && m_strip_allocas)
        continue;
    }

    if (onlyAllocSite && as != onlyAllocSite)
      continue;

    DsaAllocSite *site = m_graph.mkAllocSite(*as);
    assert(site);
    nNode.addAllocSite(*site);

    // -- update call paths on cloned allocation site
    importCallPaths(*site, n.getGraph()->getAllocSite(*as));
  }

  // -- update cache
  m_map.insert({&n, {&nNode, currentLevel}});

  // Determine if we have to copy recursively if the exact allocation site
  // object is known.
  if (onlyAllocSite && isConstantNoPtr(onlyAllocSite))
    return nNode;

  // -- recursively clone reachable nodes
  for (auto &kv : n.links()) {
    // dummy link (should not happen)
    if (kv.second->isNull())
      continue;

    // -- resolve any potential forwarding
    kv.second->getNode();
    // recursively clone the node pointed by the link
    Cell nCell(&clone(*kv.second->getNode()), kv.second->getRawOffset());
    // create new link
    nNode.setLink(kv.first, nCell);
  }

  // -- don't expect the new node to collapse
  assert(!nNode.isForwarding());

  return nNode;
}
