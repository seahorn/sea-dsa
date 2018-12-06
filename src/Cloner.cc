#include "sea_dsa/Cloner.hh"
#include "sea_dsa/support/Debug.h"
#include "llvm/IR/Instructions.h"
using namespace sea_dsa;

Node &Cloner::clone(const Node &n, bool forceAlloca) {
  // -- don't clone nodes that are already in the graph
  if (n.getGraph() == &m_graph)
    return *const_cast<Node *>(&n);

  // Calculate the cloning Step to add to Call Path.
  llvm::Optional<DSAllocSite::CallEdge> optCloningEdge = llvm::None;
  if (m_cloningContext.Direction != CloningContext::Unset) {
    const auto optCS = m_cloningContext.CS;
    assert(m_cloningContext.CS);
    assert(optCS);
    DSAllocSite::StepKind direction =
        (m_cloningContext.Direction == CloningContext::BottomUp)
            ? DSAllocSite::BottomUp
            : DSAllocSite::TopDown;
    optCloningEdge = DSAllocSite::CallEdge(direction, *m_cloningContext.CS);
  }

  // check the cache
  auto it = m_map.find(&n);
  if (it != m_map.end()) {
    Node &nNode = *(it->second);
    // if alloca are stripped but this call forces them,
    // then ensure that all allocas of n are copied over to nNode
    if (m_strip_allocas && forceAlloca &&
        nNode.getAllocSites().size() < n.getAllocSites().size()) {
      nNode.resetAllocSites();
      nNode.insertAllocSites(n.getAllocSites().begin(), n.getAllocSites().end(),
                             optCloningEdge);
    }
    return nNode;
  }

  static int cnt = 0;
  ++cnt;

  //  for (auto *AS : n.getAllocSites()) {
  //    assert(AS->getOwner() != &m_graph);
  //    if (optCloningEdge) {
  //      llvm::ImmutableCallSite cs = optCloningEdge->second;
  //      const llvm::Function *f = m_cloningContext.Direction ==
  //      CloningContext::TopDown ? cs.getCaller() : cs.getCalledFunction(); for
  //      (auto &Path : AS->getPaths()) {
  //        assert(Path.back().second == f);
  //      }
  //      if (cnt > 45080) {
  //        AS->print(llvm::errs());
  //        llvm::errs() << "\n";
  //      }
  //    }
  //  }
  //
  //  for (const auto &AS : m_graph.alloc_sites()) {
  //    assert(AS.getOwner() == &m_graph);
  //    if (optCloningEdge) {
  //      if (cnt > 45080 && AS.hasCallPaths()) {
  //        AS.print(llvm::errs());
  //        llvm::errs() << "\n----------------\n";
  //      }
  //    }
  //  }

  // -- clone the node (except for the links)
  Node &nNode = m_graph.cloneNode(n);

  //  for (auto *AS : nNode.getAllocSites()) {
  //    assert(AS->getOwner() == &m_graph);
  //    if (optCloningEdge) {
  //      if (cnt > 45080) {
  //        AS->print(llvm::errs());
  //        llvm::errs() << "\n";
  //      }
  //    }
  //  }

  nNode.resetAllocSites();

  // if not forcing allocas and stripping allocas is enabled, remove
  // all alloca instructions from the new node
  if (!forceAlloca && m_strip_allocas) {
    unsigned sz = nNode.getAllocSites().size();
    nNode.resetAllocSites();

    llvm::SmallVector<DSAllocSite *, 16> sites;
    for (DSAllocSite *as : n.getAllocSites()) {
      const llvm::Value *val = &as->getAllocSite();
      if (!llvm::isa<llvm::AllocaInst>(val))
        sites.push_back(as);
    }
    nNode.insertAllocSites(sites.begin(), sites.end(), optCloningEdge);
    LOG("cloner", if (nNode.getAllocSites().size() < sz) llvm::errs()
                      << "Clone: reduced allocas from " << sz << " to "
                      << nNode.getAllocSites().size() << "\n";);
  }
  // -- update cache
  m_map.insert({&n, &nNode});

  //  for (auto *AS : nNode.getAllocSites()) {
  //    assert(AS->getOwner() == &m_graph);
  //    if (optCloningEdge) {
  //      if (cnt > 45090) {
  //        AS->print(llvm::errs());
  //        llvm::errs() << "\n";
  //      }
  //      llvm::ImmutableCallSite cs = optCloningEdge->second;
  //      const llvm::Function *f = m_cloningContext.Direction ==
  //      CloningContext::BottomUp ? cs.getCaller() : cs.getCalledFunction();
  //      for (auto &Path : AS->getPaths()) {
  //        llvm::errs() << cnt << "\t" << &m_graph << "\n";
  //        assert(Path.back().second == f);
  //      }
  //    }
  //  }

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
