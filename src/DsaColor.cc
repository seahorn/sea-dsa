#include "sea_dsa/DsaColor.hh"

#include "sea_dsa/Graph.hh"
#include "llvm/IR/Argument.h"
#include "llvm/IR/Function.h"

#include "sea_dsa/Mapper.hh"

using namespace sea_dsa;
using namespace llvm;

// Color class

namespace sea_dsa {

void color_nodes_aux(const Node &n_callee, const Node &n_caller,
                     NodeSet &f_proc, SimulationMapper &sm, ColorMap &c_callee,
                     ColorMap &c_caller) {

  f_proc.insert(&n_callee); // mark processed

  Color col = std::rand() | 0xA0A0A0; // to avoid darker colors
  auto it = c_caller.find(&n_caller);
  if (it != c_caller.end()) {
    col = it->second;
  } else {
    c_caller.insert({&n_caller, col});
  }
  c_callee.insert({&n_callee, col});

  for (auto &links : n_callee.getLinks()) {
    const Cell &next_c_callee = *links.second;
    const Node *next_n_callee = next_c_callee.getNode();
    const Cell &next_c_caller = sm.get(next_c_callee);
    const Node *next_n_caller = next_c_caller.getNode();

    if (f_proc.count(next_n_callee) == 0) { // not processed yet
      color_nodes_aux(*next_n_callee, *next_n_caller, f_proc, sm, c_callee,
                      c_caller);
    }
  }
}

void color_nodes_graph(Graph &g, const Function &F, SimulationMapper &sm,
                       ColorMap &c_callee, ColorMap &c_caller) {

  NodeSet f_proc; // keep track of processed nodes
  // global variables are not colored
  for (const Argument &a : F.args()) {
    if (g.hasCell(a)) { // scalar arguments don't have cells
      const Cell &c = g.getCell(a);
      const Node *n = c.getNode();
      const Cell &cell_caller = sm.get(c);
      const Node *n_caller = cell_caller.getNode();
      color_nodes_aux(*n, *n_caller, f_proc, sm, c_callee, c_caller);
    }
  }
}

void colorGraph(const DsaCallSite &cs, const Graph &calleeG,
                const Graph &callerG, ColorMap &color_callee,
                ColorMap &color_caller) {

  SimulationMapper simMap;

  Graph::computeCalleeCallerMapping(
      cs, *(const_cast<Graph *>(&calleeG)), *(const_cast<Graph *>(&callerG)),
      simMap);

  color_nodes_graph(*(const_cast<Graph *>(&calleeG)),*cs.getCallee(),simMap,color_callee,color_caller);
}


} // sea_dsa
