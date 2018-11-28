#ifndef __DSA__CLONER__HH_
#define __DSA__CLONER__HH_

#include "sea_dsa/Graph.hh"

namespace sea_dsa {
/**
 * Recursively clone DSA sub-graph rooted at a given Node
 */
class Cloner {
public:
private:
  Graph &m_graph;
  llvm::DenseMap<const Node *, Node *> m_map;
  bool m_strip_alloca;

public:
  Cloner(Graph &g, bool strip = false) : m_graph(g), m_strip_alloca(strip) {}

  /// Returns a clone of a given node in the new graph
  /// Recursive clones nodes linked by this node as necessary
  Node &clone(const Node &n, bool force = false);

  /// Returns a cloned node that corresponds to the given node
  Node &at(const Node &n) {
    assert(hasNode(n));
    auto it = m_map.find(&n);
    assert(it != m_map.end());
    return *(it->second);
  }

  /// Returns true if the node has already been cloned
  bool hasNode(const Node &n) { return m_map.count(&n) > 0; }
};
} // namespace sea_dsa

#endif
