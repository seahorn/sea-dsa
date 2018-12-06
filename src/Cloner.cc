#include "sea_dsa/Cloner.hh"
#include "sea_dsa/support/Debug.h"
#include "llvm/IR/Instructions.h"
using namespace sea_dsa;

Node &Cloner::clone(const Node &n, bool forceAlloca) {
  // -- don't clone nodes that are already in the graph
  if (n.getGraph() == &m_graph)
    return *const_cast<Node *>(&n);

  // Calculate the cloning Step to add to Call Path.
  llvm::Optional<DSAllocSite::Step> optCloningStep = llvm::None;
  if (m_cloningContext.Direction != CloningContext::Unset) {
    const auto optCS = m_cloningContext.CS;
    assert(m_cloningContext.CS);
    assert(optCS);
    DSAllocSite::StepKind direction;
    const llvm::Function *dest = nullptr;
    switch (m_cloningContext.Direction) {
    case CloningContext::BottomUp: {
      direction = DSAllocSite::BottomUp;
      if (optCS.hasValue())
        dest = DsaCallSite(*optCS).getCaller();
      break;
    }
    case CloningContext::TopDown: {
      direction = DSAllocSite::TopDown;
      if (optCS.hasValue())
        dest = DsaCallSite(*optCS).getCallee();
      break;
    }
    case CloningContext::Unset:
      break;
    default:
      llvm_unreachable("Unhandled case");
    }
    optCloningStep =
        DSAllocSite::Step(direction, const_cast<llvm::Function *>(dest));
  }

  // check the cache
  auto it = m_map.find(&n);
  if (it != m_map.end()) {
    Node &nNode = *(it->second);
    // if alloca are stripped but this call forces them,
    // then ensure that all allocas of n are copied over to nNode
    if (m_strip_allocas && forceAlloca &&
        nNode.getAllocSites().size() < n.getAllocSites().size()) {
      nNode.insertAllocSites(n.getAllocSites().begin(), n.getAllocSites().end(),
                             optCloningStep);
    }
    return nNode;
  }

  // -- clone the node (except for the links)
  Node &nNode = m_graph.cloneNode(n);

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
    nNode.insertAllocSites(sites.begin(), sites.end(), optCloningStep);
    LOG("cloner", if (nNode.getAllocSites().size() < sz) llvm::errs()
                      << "Clone: reduced allocas from " << sz << " to "
                      << nNode.getAllocSites().size() << "\n";);
  }
  // -- update cache
  m_map.insert({&n, &nNode});

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
