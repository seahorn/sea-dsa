#pragma once

#include "sea_dsa/Graph.hh"
#include "llvm/IR/Function.h"

namespace sea_dsa {
namespace graph_utils {

template <typename Set>
static void markReachableNodes(const Node *n, Set &set) {
  if (!n)
    return;
  assert(!n->isForwarding() && "Cannot mark a forwarded node");

  if (set.insert(n).second)
    for (auto const &edg : n->links())
      markReachableNodes(edg.second->getNode(), set);
}

template <typename Set>
static void reachableNodes(const llvm::Function &fn, Graph &g, Set &inputReach,
                           Set &retReach) {
  // formal parameters
  for (const llvm::Value &arg : fn.args()) {
    if (g.hasCell(arg)) {
      Cell &c = g.mkCell(arg, Cell());
      markReachableNodes(c.getNode(), inputReach);
    }
  }

  // globals
  for (auto &kv : g.globals())
    markReachableNodes(kv.second->getNode(), inputReach);

  // return value
  if (g.hasRetCell(fn))
    markReachableNodes(g.getRetCell(fn).getNode(), retReach);
}

} // namespace graph_utils
} // namespace sea_dsa
