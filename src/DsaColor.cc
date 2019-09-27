#include "sea_dsa/DsaColor.hh"

using namespace sea_dsa;
using namespace llvm;

// Color class

Color::Color(): m_r(0), m_g(0), m_b(0) {
  randColor();
}

Color::Color(int r, int g, int b) : m_r(r), m_g(g), m_b(b) {};

void Color::randColor() { // it would be nice to not generate the same colors all the time
  m_r = std::rand() % 255;
  m_g = std::rand() % 255;
  m_b = std::rand() % 255;
}

std::string Color::stringColor() const {
  std::ostringstream stringStream;

  stringStream << "\"#" << std::hex << m_r << m_g << m_b << "\"";
  return stringStream.str();
}


// What I actually want is a constructor that copies a graph but adds the two fields!
// Actually the copy is not necessary, I just want a pointer to the graph.
// ColorGraph class
ColoredGraph::ColoredGraph(Graph &g, ColorMap &colorM, SafeNodeSet &safe)
    : m_g(g), m_color(colorM), m_safe(safe) {};


std::string ColoredGraph::getColorNode(const Node *n) const {

  std::string color_string;
  raw_string_ostream OS(color_string);

  auto it = m_color.find(n);

  // if it is not colored, it means that it was not mapped --> return gray
  if(it != m_color.end()){
    return it->getSecond().stringColor();
  }
  else{
    OS << "grey";
    return OS.str();
  }
}

bool ColoredGraph::isSafeNode(const Node *n) const {  return m_safe.count(n) > 0; }

// Coloring functions

std::unique_ptr<Graph> cloneGraph(const llvm::DataLayout &dl,
                                  Graph::SetFactory &sf,const Graph &g) {
  std::unique_ptr<Graph> new_g( new Graph( dl, sf, g.isFlat()));
  new_g->import(g, true /*copy all parameters*/);
  return std::move(new_g);
}

// TODO: check Node.isModified() later to decide what to copy

bool GraphExplorer::isSafeCell(SafeCellSet &f_safe, const Cell *c) {
  return f_safe.count(c) > 0;
}
bool GraphExplorer::isSafeNode(SafeNodeSet &f_safe, const Node *n) {
  return f_safe.count(n) > 0;
}

void GraphExplorer::mark_cells_graph(Graph &g, const Function &F,
                                   SafeCellSet &f_safe,
                                   ExplorationMap &f_explored) {

  for (const Argument &a : F.args()) {
    if (g.hasCell(a)) { // scalar arguments don't have cells
      const Cell &c = g.getCell(a);
      mark_copy(c, f_explored, f_safe);
    }
  }
}

bool GraphExplorer::mark_copy(const Cell &v, ExplorationMap &f_color,
                            SafeCellSet &f_safe) {

  f_color[&v] = GRAY;
  const Node* n = v.getNode();

  for (auto &links : n->getLinks()){
    const Field &f = links.first;
    const Cell& next_c = *links.second;
//    if(&next_c == &v) // ask Jorge
  //    continue;
    auto it = f_color.find(&next_c);
    if (it == f_color.end() && mark_copy(next_c, f_color, f_safe)) {
        return true;
    } else if (it != f_color.end() &&  it->getSecond() == GRAY) {
      propagate_not_copy(v, f_color, f_safe);
      return true;
    }
  }

  f_color[&v] = BLACK;

  if (f_safe.count(&v) == 0)
    f_safe.insert(&v);

  return false;
}

void GraphExplorer::propagate_not_copy(const Cell &v, ExplorationMap &f_color,
                                     SafeCellSet &f_safe) {
  if(isSafeCell(f_safe,&v))
    f_safe.erase(&v);

  f_color[&v] = BLACK; // TODO: change by insert?

  const Node* n = v.getNode();
  for (auto &links : n->getLinks()){
    const Field &f = links.first;
    const Cell &next_c = *links.second;

    auto next = f_color.find(&next_c);

    bool explored = next != f_color.end() && next->getSecond() == BLACK;
    bool marked_safe = isSafeCell(f_safe, &next_c);

    if (!(explored && !marked_safe))
      propagate_not_copy(next_c, f_color, f_safe);
  }
}

void GraphExplorer::colorGraph(const DsaCallSite &cs, const Graph &calleeG,
                             const Graph &callerG, ColorMap &color_callee,
                             ColorMap &color_caller, SafeNodeSet &f_node_safe) {

  SimulationMapper simMap;

  bool res = Graph::computeCalleeCallerMapping(cs, *(const_cast<Graph*>(&calleeG)), *(const_cast<Graph*>(&callerG)), simMap); /// WARNING!

  SafeCellSet f_cell_safe;
  SafeNodeSet f_node_safe_caller;

  ExplorationMap f_explored;
  mark_cells_graph(*(const_cast<Graph *>(&calleeG)),*cs.getCallee(), f_cell_safe, f_explored);

  // init f_node_safe
  for (auto it = calleeG.begin(), end = calleeG.end(); it != end; it++) {
    const Node &n = *it->getNode();
    if (!isSafeNode(f_node_safe,&n)) // not inserted already
      f_node_safe.insert(&n);
    const Node &n_caller = *simMap.get(*it).getNode();
    if(f_node_safe_caller.count(&n_caller) == 0)
       f_node_safe_caller.insert(&n_caller);
  }


  for (auto kv = f_explored.begin(), end = f_explored.end(); kv != end; kv++) {
    const Cell * c_callee = kv->getFirst();
    const Node * n_callee = c_callee->getNode();
    const Cell &c_caller = simMap.get(*c_callee);
    const Node * n_caller = c_caller.getNode();

    Color col;
    auto it = color_caller.find(n_caller);
    if (it != color_caller.end()) {
      col = it->second; // color_caller[n_caller];
    } else {
      color_caller.insert(std::make_pair(n_caller, col));
    }

    color_callee[n_callee]=col;

    if(!isSafeCell(f_cell_safe,c_callee)){
      // remove the caller node from the safe set
      const Node &n_caller = *simMap.get(*c_callee).getNode();
      if (isSafeNode(f_node_safe_caller,&n_caller))
        f_node_safe_caller.erase(&n_caller);
    }
  }

  // mark all the nodes of the bu as not safe
  for (auto kv = f_explored.begin(), end = f_explored.end(); kv != end; kv++) {
    const Cell *c_callee = kv->getFirst();
    const Node *n_callee = c_callee->getNode();
    const Cell &c_caller = simMap.get(*c_callee);
    const Node *n_caller = c_caller.getNode();

    if(!isSafeNode(f_node_safe_caller,n_caller))
      if (isSafeNode(f_node_safe,n_callee))
        f_node_safe.erase(n_callee);
  }
}
