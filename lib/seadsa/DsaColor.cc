#include "seadsa/DsaColor.hh"

#include "seadsa/Graph.hh"
#include "llvm/IR/Argument.h"
#include "llvm/IR/Function.h"

#include "seadsa/Mapper.hh"

using namespace seadsa;
using namespace llvm;

// Color class

namespace seadsa {

void color_nodes_aux(const Node &n_callee, const Node &n_caller,
                     NodeSet &f_proc, SimulationMapper &sm, ColorMap &c_callee,
                     ColorMap &c_caller) {

  f_proc.insert(&n_callee); // mark processed

  Color col;
  auto it = c_caller.find(&n_caller);
  if (it != c_caller.end()) {
    col = it->second;
  } else {
    col = ((Color) std::rand()) << 32;
    col = ((col | std::rand()) | 0x808080) & 0xFFFFFF;
    // std::rand() is not guaranteed to be big enough (MAX_RAND is 0x7FFF)
    // 0x808080 to avoid darker colors
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
  for (auto &kv : g.globals()) {
    Cell &c = *kv.second;
    const Node *n = c.getNode();
    const Cell &cell_caller = sm.get(c);
    const Node *n_caller = cell_caller.getNode();
    color_nodes_aux(*n, *n_caller, f_proc, sm, c_callee, c_caller);
  }

  if (g.hasRetCell(F)) {
    Cell &c = g.getRetCell(F);
    const Node *n = c.getNode();
    const Cell &cell_caller = sm.get(c);
    const Node *n_caller = cell_caller.getNode();
    color_nodes_aux(*n, *n_caller, f_proc, sm, c_callee, c_caller);
  }
}

void colorGraphsCallSite(const DsaCallSite &cs, const Graph &calleeG,
                         const Graph &callerG, ColorMap &color_callee,
                         ColorMap &color_caller) {

  SimulationMapper simMap;

  Graph::computeCalleeCallerMapping(
      cs, *(const_cast<Graph *>(&calleeG)), *(const_cast<Graph *>(&callerG)),
      simMap);

  color_nodes_graph(*(const_cast<Graph *>(&calleeG)), *cs.getCallee(), simMap,
                    color_callee, color_caller);
}

void colorGraphsFunction(const Function &f, const Graph &fromG,
                         const Graph &toG, ColorMap &colorFrom,
                         ColorMap &colorTo) {
  SimulationMapper sm;

  Graph::computeSimulationMapping(*(const_cast<Graph *>(&fromG)),
				  *(const_cast<Graph *>(&toG)), sm);
  color_nodes_graph(*(const_cast<Graph *>(&fromG)), f, sm, colorFrom, colorTo);
}
} // namespace seadsa
