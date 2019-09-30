#pragma once

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/GraphTraits.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"

#include "sea_dsa/BottomUp.hh"
#include "sea_dsa/CallSite.hh"
#include "sea_dsa/Global.hh"
#include "sea_dsa/Graph.hh"
#include "sea_dsa/GraphTraits.hh"

using namespace sea_dsa;
using namespace llvm;

class Color {
private:
  int m_r, m_g, m_b;

public:
  Color();
  Color(int r, int g, int b);

  void randColor();
  std::string stringColor() const;
};

enum e_color {WHITE, BLACK, GRAY}; // colors for exploration

using ExplorationMap = DenseMap<const Cell *, e_color>;
using ColorMap = DenseMap<const Node *, Color>;
using SafeCellSet = DenseSet<const Cell *>;
using SafeNodeSet = DenseSet<const Node *>;

namespace sea_dsa {

// This class is used to print a dsa graph with colored nodes. The coloring is
// done by GraphExplorer::colorGraph.
class ColoredGraph {
private:
  SafeNodeSet m_safe;
  ColorMap m_color;
  Graph &m_g;

public:
  ColoredGraph(Graph &g, ColorMap &colorM, SafeNodeSet &safe);
  sea_dsa::Graph & getGraph();
  sea_dsa::Graph &getGraph() const;

  std::string getColorNode(const Node *n) const;
  bool isSafeNode(const Node *n) const;

  typedef sea_dsa::Graph::iterator iterator;
};
} // end namespace sea_dsa

// To store the state of the graph after the bu pass
std::unique_ptr<Graph> cloneGraph(const llvm::DataLayout &dl,
                                  Graph::SetFactory &sf, const Graph &g);

class GraphExplorer {
private:

  static void
  mark_cells_graph(Graph &g, const Function &F, SafeCellSet &f_safe,
                   ExplorationMap &f_visited); // f_visited is useful to avoid
                                                // computing reachability again

  static bool mark_copy(const Cell &v, ExplorationMap &f_color, SafeCellSet &f_safe);
  static void propagate_not_copy(const Cell &v, ExplorationMap &f_color,
                          SafeCellSet &f_safe);
  static bool isSafeCell(SafeCellSet &f_safe, const Cell *c);
  static bool isSafeNode(SafeNodeSet &f_safe, const Node *n);

public:
  // This method takes a callsite, and a pair of dsa graphs, one of the caller
  // and one of the callee, and assigns the same color to all the nodes of the
  // callee's graph that are simulated as the node (with the same color) in the
  // caller's graph.
  //
  // For each of the nodes in the callee's graph, f_node_safe stores whether the
  // amount of information that it may encode is infinite or finite.
  static void colorGraph(const DsaCallSite &cs, const Graph &g_callee,
                  const Graph &g_caller, ColorMap &color_callee,
                  ColorMap &color_caller, SafeNodeSet &f_node_safe);
};

namespace llvm {
  // TODO: move to some header file?
  template <> struct GraphTraits<sea_dsa::ColoredGraph *> {
    typedef sea_dsa::Node NodeType;
    typedef sea_dsa::Node::iterator ChildIteratorType;
    typedef sea_dsa::ColoredGraph::iterator nodes_iterator;

    static nodes_iterator nodes_begin(sea_dsa::ColoredGraph *G) {
      return G->getGraph().begin();
    }
    static nodes_iterator nodes_end(sea_dsa::ColoredGraph *G) {
      return G->getGraph().end();
    }

    static ChildIteratorType child_begin(NodeType *N) { return N->begin(); }
    static ChildIteratorType child_end(NodeType *N) { return N->end(); }
};

template <> struct GraphTraits<const sea_dsa::ColoredGraph*> {
  typedef const sea_dsa::Node NodeType;
  typedef sea_dsa::Node::const_iterator ChildIteratorType;

  // nodes_iterator/begin/end - Allow iteration over all nodes in the graph
  typedef sea_dsa::Graph::const_iterator nodes_iterator;

  static nodes_iterator nodes_begin(const sea_dsa::ColoredGraph *G) {
    return G->getGraph().begin();
  }
  static nodes_iterator nodes_end(const sea_dsa::ColoredGraph *G) {
    return G->getGraph().end();
  }

  static ChildIteratorType child_begin(NodeType *N) { return N->begin(); }
  static ChildIteratorType child_end(NodeType *N) { return N->end(); }
};

} // end llvm
