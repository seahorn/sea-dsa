#include "sea_dsa/TopDown.hh"

#include "llvm/ADT/SCCIterator.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/IR/CallSite.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"

#include "sea_dsa/AllocWrapInfo.hh"
#include "sea_dsa/CallSite.hh"
#include "sea_dsa/Cloner.hh"
#include "sea_dsa/Graph.hh"
#include "sea_dsa/Local.hh"
#include "sea_dsa/config.h"
#include "sea_dsa/support/Debug.h"

#include "boost/range/iterator_range.hpp"

using namespace llvm;

namespace sea_dsa {

// Clone caller nodes into callee and resolve arguments
// XXX: this code is pretty much symmetric to the one defined in
// BottomUp. They should be merged at some point.
void TopDownAnalysis::cloneAndResolveArguments(const DsaCallSite &cs,
                                               Graph &callerG, Graph &calleeG,
                                               bool noescape) {
  CloningContext context(*cs.getInstruction(), CloningContext::TopDown);
  auto options = Cloner::BuildOptions(Cloner::StripAllocas);
  Cloner C(calleeG, context, options);

  // clone and unify globals
  for (auto &kv : boost::make_iterator_range(callerG.globals_begin(),
                                             callerG.globals_end())) {
    Node &n = C.clone(*kv.second->getNode());
    Cell c(n, kv.second->getRawOffset());
    Cell &nc = calleeG.mkCell(*kv.first, Cell());
    nc.unify(c);
  }

  // clone and unify return
  const Function &callee = *cs.getCallee();
  if (calleeG.hasRetCell(callee) && callerG.hasCell(*cs.getInstruction())) {
    auto &inst = *cs.getInstruction();
    const Cell &csCell = callerG.getCell(inst);
    Node &n = C.clone(*csCell.getNode());
    Cell c(n, csCell.getRawOffset());
    Cell &nc = calleeG.getRetCell(callee);
    nc.unify(c);
  }

  // clone and unify actuals and formals
  DsaCallSite::const_actual_iterator AI = cs.actual_begin(),
                                     AE = cs.actual_end();
  for (DsaCallSite::const_formal_iterator FI = cs.formal_begin(),
                                          FE = cs.formal_end();
       FI != FE && AI != AE; ++FI, ++AI) {
    const Value *arg = (*AI).get();
    const Value *fml = &*FI;
    if (callerG.hasCell(*arg) && calleeG.hasCell(*fml)) {
      const Cell &callerCell = callerG.getCell(*arg);
      Node &n = C.clone(*callerCell.getNode(), noescape);
      Cell c(n, callerCell.getRawOffset());
      Cell &nc = calleeG.mkCell(*fml, Cell());
      nc.unify(c);
    }
  }
  calleeG.compress();
}

bool TopDownAnalysis::runOnModule(Module &M, GraphMap &graphs) {

  LOG("dsa-td", errs() << "Started top-down analysis ... \n");

  // The SCC iterator has the property that the graph is traversed in
  // post-order.
  //
  // Here, we want to visit all callers before callees. We could use
  // SCC iterator on the Inverse graph but it requires to implement
  // some wrappers around CallGraph. Instead, we store the postorder
  // SCC in a vector and traverse in reversed order.
  typedef scc_iterator<CallGraph *> scc_iterator_t;
  typedef std::vector<typename GraphTraits<CallGraph *>::NodeRef> scc_t;

  std::vector<scc_t> postorder_scc;

  // copy all SCC elements in the vector
  // XXX: this is inefficient
  postorder_scc.reserve(std::distance(m_cg.begin(), m_cg.end()));
  for (auto it = scc_begin(&m_cg); !it.isAtEnd(); ++it) {
    postorder_scc.push_back(*it);
  }

  for (auto it = postorder_scc.rbegin(), et = postorder_scc.rend(); it != et;
       ++it) {
    auto &scc = *it;
    for (CallGraphNode *cgn : scc) {
      Function *fn = cgn->getFunction();
      if (!fn || fn->isDeclaration() || fn->empty())
        continue;
      // -- resolve all function calls in the SCC
      for (auto &callRecord : *cgn) {
        ImmutableCallSite CS(callRecord.first);
        DsaCallSite dsaCS(CS);
        const Function *callee = dsaCS.getCallee();
        if (!callee || callee->isDeclaration() || callee->empty())
          continue;

        // XXX: We assume that `graphs` has been already populated by
        // the bottom-up pass. We report an error and skip the
        // callsite otherwise.
        auto it = graphs.find(dsaCS.getCaller());
        if (it == graphs.end()) {
          errs() << "ERROR: top-down analysis could not find dsa graph for "
                    "caller\n";
          continue;
        }
        Graph &callerG = *(it->second);
        it = graphs.find(dsaCS.getCallee());
        if (it == graphs.end()) {
          errs() << "ERROR: top-down analysis could not find dsa graph for "
                    "callee\n";
          continue;
        }
        Graph &calleeG = *(it->second);
        // propagate from the caller to the callee
        cloneAndResolveArguments(dsaCS, callerG, calleeG, m_noescape);
      }
    }
  }

  LOG("dsa-td-graph", for (auto &kv
                           : graphs) {
    errs() << "### Top-down Dsa graph for " << kv.first->getName() << "\n";
    kv.second->write(errs());
    errs() << "\n";
  });

  LOG("dsa-td", errs() << "Finished top-down analysis\n");
  return false;
}
} // namespace sea_dsa
