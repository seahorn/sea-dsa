#pragma once

#include "sea_dsa/Graph.hh"

namespace sea_dsa {

struct CloningContext {
  llvm::Optional<const llvm::Instruction *> m_cs;
  enum DirectionKind { Unset, BottomUp, TopDown };
  DirectionKind m_dir;

  CloningContext(const llvm::Instruction &cs, DirectionKind dir)
      : m_cs(&cs), m_dir(dir) {}

  static CloningContext mkNoContext() { return {}; }

private:
  CloningContext() : m_cs(llvm::None), m_dir(DirectionKind::Unset) {}
};

/**
 * \brief Recursively clone DSA sub-graph rooted at a given Node
 */
class Cloner {
public:
  enum Options : unsigned {
    Basic = 0,
    StripAllocas = 1 << 0,
    TrackAllocaCallPaths = 1 << 1,
  };

  template <typename... Os> static Options BuildOptions(Os... os) {
    Options options = Basic;
    Options unpacked[] = {os...};
    for (Options o : unpacked)
      options = Options(options | o);
    return options;
  }

private:
  Graph &m_graph;
  llvm::DenseMap<const Node *, Node *> m_map;
  CloningContext m_context;
  bool m_track_call_paths;
  bool m_strip_allocas;

  bool isTopDown() const { return m_context.m_dir == CloningContext::TopDown; }
  bool isBottomUp() const {
    return m_context.m_dir == CloningContext::BottomUp;
  }
  bool isUnset() const { return !(isTopDown() || isBottomUp()); }

  void importCallPaths(DsaAllocSite &site,
                       llvm::Optional<DsaAllocSite *> other);

public:
  Cloner(Graph &g, CloningContext context, Cloner::Options options)
      : m_graph(g), m_context(context),
        m_strip_allocas(options & Cloner::Options::StripAllocas),
        m_track_call_paths(options & Cloner::Options::TrackAllocaCallPaths) {}

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
