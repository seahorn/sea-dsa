#include "sea_dsa/DsaColor.hh"

using namespace sea_dsa;
using namespace llvm;

// Color class

Color::Color(): m_r(0), m_g(0), m_b(0) {
  randColor();
}

Color::Color(int r, int g, int b) : m_r(r), m_g(g), m_b(b) {};

void Color::randColor() {
  m_r = std::rand() % 255;
  m_g = std::rand() % 255;
  m_b = std::rand() % 255;
}

std::string Color::stringColor() const {
  std::ostringstream stringStream;

  stringStream << "\"#" << std::hex << m_r << m_g << m_b << "\"";
  return stringStream.str();
}

bool GraphExplorer::isSafeNode(NodeSet &f_safe, const Node *n) {
  return f_safe.count(n) == 0;
}

void GraphExplorer::mark_nodes_graph(Graph &g, const Function &F,
                                     NodeSet &f_safe, NodeSet &f_safe_caller,
                                     SimulationMapper &sm) {
  ExplorationMap f_visited;

  for (const Argument &a : F.args()) {
    if (g.hasCell(a)) { // scalar arguments don't have cells
      const Cell &c = g.getCell(a);
      const Node *n = c.getNode();
      mark_copy(*n, f_visited, f_safe, f_safe_caller, sm);
    }
  }
}

bool GraphExplorer::mark_copy(const Node &n, ExplorationMap &f_color,
                              NodeSet &f_safe, NodeSet &f_safe_caller,
                              SimulationMapper &sm ) {
  f_color[&n] = EColor::GRAY;

  for (auto &links : n.getLinks()) {
    const Field &f = links.first;
    const Cell &next_c = *links.second;
    const Node *next_n = next_c.getNode();
    auto it = f_color.find(next_n);
    if (next_n->isArray()){ // encodes an object of unbounded size
      propagate_not_copy(n, f_color, f_safe, f_safe_caller, sm);
      return true;
    }
    else if (it == f_color.end() && mark_copy(*next_n, f_color, f_safe,f_safe_caller,sm)) {
      return true;
    } else if (it != f_color.end() && it->getSecond() == EColor::GRAY) {
      propagate_not_copy(n, f_color, f_safe,f_safe_caller,sm);
      return true;
    }
  }

  f_color[&n] = EColor::BLACK;

  return false;
}

void GraphExplorer::propagate_not_copy(const Node &n, ExplorationMap &f_color,
                                       NodeSet &f_safe,
                                       NodeSet &f_safe_caller,
                                       SimulationMapper &sm) {
  if(isSafeNode(f_safe,&n))
    f_safe.insert(&n); // we store the ones that are not safe

  f_color[&n] = EColor::BLACK;

  for (auto &links : n.getLinks()){
    const Field &f = links.first;
    const Cell &next_c = *links.second;
    const Node * next_n = next_c.getNode();

    auto next = f_color.find(next_n);

    bool explored = next != f_color.end() && next->getSecond() == EColor::BLACK;
    bool marked_safe = isSafeNode(f_safe, next_n);

    if (!(explored && !marked_safe)) {
      const Node &next_n_caller = *sm.get(next_c).getNode();

      if(isSafeNode(f_safe_caller,&next_n_caller))
         f_safe_caller.insert(&next_n_caller);

      propagate_not_copy(*next_n, f_color, f_safe, f_safe_caller, sm);
    }
  }
}

void GraphExplorer::color_nodes_graph(Graph &g, const Function &F,
                                      SimulationMapper &sm, ColorMap &c_callee,
                                      ColorMap &c_caller,
                                      NodeSet f_node_safe_callee,
                                      NodeSet f_node_safe_caller) {

  NodeSet f_proc; // keep track of processed nodes
  for (const Argument &a : F.args()) {
    if (g.hasCell(a)) { // scalar arguments don't have cells
      const Cell &c = g.getCell(a);
      const Node *n = c.getNode();
      const Cell &cell_caller = sm.get(c);
      const Node *n_caller = cell_caller.getNode();
      color_nodes_aux(*n, *n_caller,f_proc, sm, c_callee, c_caller,
                      f_node_safe_callee, f_node_safe_caller);
    }
  }
}

void GraphExplorer::color_nodes_aux(const Node &n_callee, const Node &n_caller, NodeSet &f_proc,
                                    SimulationMapper &sm, ColorMap &c_callee,
                                    ColorMap &c_caller,
                                    NodeSet f_node_safe_callee,
                                    NodeSet f_node_safe_caller) {

  f_proc.insert(&n_callee); // mark processed

  Color col;
  auto it = c_caller.find(&n_caller);
  if (it != c_caller.end()) {
    col = it->second;
  } else {
    c_caller.insert(std::make_pair(&n_caller, col));
  }
  c_callee.insert(std::make_pair(&n_callee, col));

  for (auto &links : n_callee.getLinks()) {
    const Field &f = links.first;
    const Cell &next_c_callee = *links.second;
    const Node * next_n_callee = next_c_callee.getNode();
    const Cell &next_c_caller = sm.get(next_c_callee);
    const Node *next_n_caller = next_c_caller.getNode();

    // update if the node is safe (w.r.t. the simulated node)
    if (!isSafeNode(f_node_safe_caller, next_n_caller)) {
      // add the node to the unsafe set
      if (isSafeNode(f_node_safe_caller, next_n_callee))
        f_node_safe_callee.insert(next_n_caller);
    }
    if (f_proc.count(next_n_callee) == 0) { // not processed yet
      color_nodes_aux(*next_n_callee, *next_n_caller, f_proc, sm, c_callee,
                      c_caller, f_node_safe_callee, f_node_safe_caller);
    }
  }
}

void GraphExplorer::colorGraph(const DsaCallSite &cs, const Graph &calleeG,
                             const Graph &callerG, ColorMap &color_callee,
                               ColorMap &color_caller, NodeSet &f_node_safe_callee) {

  SimulationMapper simMap;

  bool res = Graph::computeCalleeCallerMapping(cs, *(const_cast<Graph*>(&calleeG)), *(const_cast<Graph*>(&callerG)), simMap);
  NodeSet f_node_safe_caller;

  mark_nodes_graph(*(const_cast<Graph *>(&calleeG)), *cs.getCallee(),
                   f_node_safe_callee, f_node_safe_caller, simMap);

  color_nodes_graph(*(const_cast<Graph *>(&calleeG)),*cs.getCallee(),simMap,color_callee,color_caller,f_node_safe_callee,f_node_safe_caller);
}

/************************************************************/
/* only safe exploration, no coloring!                      */
/************************************************************/

void GraphExplorer::getUnsafeNodesCallSite(const DsaCallSite &cs,
                                           const Graph &calleeG,
                                           const Graph &callerG,
                                           SimulationMapper &simMap,
                                           NodeSet &f_node_safe_callee,
                                           NodeSet &f_node_safe_caller) {

  //  DsaCallSite dsa_cs(*cs.getInstruction());

  mark_nodes_graph(*(const_cast<Graph *>(&calleeG)), *cs.getCallee(),
                   f_node_safe_callee, f_node_safe_caller, simMap);
}
