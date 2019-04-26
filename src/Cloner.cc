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
  return;
}

/// Return true if the instruction allocates memory on the stack
static bool isStackAllocation(const llvm::Value *v) {
  // XXX Check whether there are other ways to allocate stack, like
  // insertvalue/extractvalue
  return llvm::isa<llvm::AllocaInst>(v);
}

Node &Cloner::clone(const Node &n, bool forceAlloca) {
  // -- don't clone nodes that are already in the graph
  if (n.getGraph() == &m_graph)
    return *const_cast<Node *>(&n);

  // check the cache
  auto it = m_map.find(&n);
  if (it != m_map.end()) {
    Node &nNode = *(it->second->getNode());
    // if alloca are stripped but this call forces them,
    // then ensure that all allocas of n are copied over to nNode
    if (m_strip_allocas && forceAlloca &&
        nNode.getAllocSites().size() < n.getAllocSites().size()) {
      for (const llvm::Value *as : n.getAllocSites()) {
        DsaAllocSite *site = m_graph.mkAllocSite(*as);
        assert(site);
        nNode.addAllocSite(*site);

        // -- update call paths on the cloned allocation site
        importCallPaths(*site, n.getGraph()->getAllocSite(*as));
      }
    }
    return nNode;
  }

  // -- clone the node (except for the links)
  Node &nNode = m_graph.cloneNode(n, false /* cpAllocSites */);

  // -- copy allocation sites, except stack based, unless requested
  for (const llvm::Value *as : n.getAllocSites()) {
    if (isStackAllocation(as)) {
      if (!forceAlloca && m_strip_allocas)
        continue;
    }

    DsaAllocSite *site = m_graph.mkAllocSite(*as);
    assert(site);
    nNode.addAllocSite(*site);

    // -- update call paths on cloned allocation site
    importCallPaths(*site, n.getGraph()->getAllocSite(*as));
  }

  // -- update cache
  m_map.insert(std::make_pair(&n, &nNode));

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
