#include "seadsa/BottomUp.hh"

#include "llvm/ADT/SCCIterator.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

#include "seadsa/AllocWrapInfo.hh"
#include "seadsa/CallGraphUtils.hh"
#include "seadsa/CallSite.hh"
#include "seadsa/Cloner.hh"
#include "seadsa/DsaLibFuncInfo.hh"
#include "seadsa/Graph.hh"
#include "seadsa/Local.hh"
#include "seadsa/config.h"
#include "seadsa/support/Debug.h"

using namespace llvm;

namespace seadsa {
bool NoBUFlowSensitiveOpt;
}

static llvm::cl::opt<bool, true> XNoBUFlowSensitiveOpt(
    "sea-dsa-no-bu-flow-sensitive-opt",
    llvm::cl::desc("Disable partial flow sensitivity in bottom up"),
    llvm::cl::location(seadsa::NoBUFlowSensitiveOpt), llvm::cl::init(false),
    llvm::cl::Hidden);

namespace seadsa {

static const Value *findUniqueReturnValue(const Function &F) {
  const Value *onlyRetVal = nullptr;

  for (const auto &BB : F) {
    auto *TI = BB.getTerminator();
    auto *RI = dyn_cast<ReturnInst>(TI);
    if (!RI) continue;

    const Value *rv = RI->getOperand(0)->stripPointerCasts();
    if (onlyRetVal && onlyRetVal != rv) return nullptr;

    onlyRetVal = rv;
  }

  return onlyRetVal;
}

// Clone callee nodes into caller and resolve arguments
void BottomUpAnalysis::cloneAndResolveArguments(
    const DsaCallSite &CS, Graph &calleeG, Graph &callerG,
    const DsaLibFuncInfo &dsaLibFuncInfo, bool flowSensitiveOpt) {
  CloningContext context(*CS.getInstruction(), CloningContext::BottomUp);
  auto options = Cloner::BuildOptions(Cloner::StripAllocas);
  Cloner C(callerG, context, options);
  assert(context.m_cs);

  // clone and unify globals
  for (auto &kv : calleeG.globals()) {
    Node &calleeN = *kv.second->getNode();
    // We don't care if globals got unified together, but have to respect the
    // points-to relations introduced by the callee introduced.
#if 0
    if (flowSensitiveOpt)
      if (calleeN.getNumLinks() == 0 || !calleeN.isModified() ||
          llvm::isa<ConstantData>(kv.first))
        continue;
#endif

    Node &n = C.clone(calleeN, false, (!flowSensitiveOpt ? nullptr : kv.first));
    Cell c(n, kv.second->getRawOffset());
    Cell &nc = callerG.mkCell(*kv.first, Cell());
    nc.unify(c);
  }

  // clone and unify return
  const Function &callee = dsaLibFuncInfo.hasSpecFunc(*CS.getCallee())
                               ? *dsaLibFuncInfo.getSpecFunc(*CS.getCallee())
                               : *CS.getCallee();
  if (calleeG.hasRetCell(callee)) {
    Cell &nc = callerG.mkCell(*CS.getInstruction(), Cell());

    // If the return value corresponds to a global then it should
    // propagate to the caller that global, regardless of the
    // unifications in the callee graph.
    const Value *onlyAllocSite = findUniqueReturnValue(callee);
    if (!flowSensitiveOpt ||
        (onlyAllocSite && !isa<GlobalValue>(onlyAllocSite))) {
      onlyAllocSite = nullptr;
    }

    const Cell &ret = calleeG.getRetCell(callee);
    Node &n = C.clone(*ret.getNode(), false, onlyAllocSite);
    Cell c(n, ret.getRawOffset());
    nc.unify(c);

    // Unify the cloned global with the global in the caller graph.
    if (onlyAllocSite) {
      assert(isa<GlobalValue>(onlyAllocSite));
      Cell &nc = callerG.mkCell(*onlyAllocSite, Cell());
      nc.unify(c);
    }
  }

  auto range = llvm::make_filter_range(
      callee.args(), [](auto &arg) { return arg.getType()->isPointerTy(); });

  DsaCallSite::const_actual_iterator AI = CS.actual_begin(),
                                     AE = CS.actual_end();
  for (auto FI = range.begin(), FE = range.end(); FI != FE && AI != AE;
       ++FI, ++AI) {
    const Value *arg = (*AI).get();
    const Value *fml = &*FI;
    if (calleeG.hasCell(*fml)) {
      const Cell &formalC = calleeG.getCell(*fml);
      Node &n = C.clone(*formalC.getNode());
      n.setExternal(false);
      Cell c(n, formalC.getRawOffset());
      Cell &nc = callerG.mkCell(*arg, Cell());
      nc.unify(c);
    }
  }

  callerG.compress();
}

bool BottomUpAnalysis::runOnModule(Module &M, GraphMap &graphs) {

  LOG("dsa-bu", errs() << "Started bottom-up analysis ... \n");

  LocalAnalysis la(m_dl, m_tliWrapper, m_allocInfo);

  for (auto it = scc_begin(&m_cg); !it.isAtEnd(); ++it) {
    auto &scc = *it;

    // -- compute a local graph shared between all functions in the scc
    GraphRef fGraph = nullptr;
    for (CallGraphNode *cgn : scc) {
      Function *fn = cgn->getFunction();
      if (!fn) continue;
      if (fn->isDeclaration() && !m_dsaLibFuncInfo.hasSpecFunc(*fn)) continue;

      if (!fGraph) {
        assert(graphs.find(fn) != graphs.end());
        fGraph = graphs[fn];
        assert(fGraph);
      }
      if (m_dsaLibFuncInfo.hasSpecFunc(*fn)) {
        Function &spec_fn = *m_dsaLibFuncInfo.getSpecFunc(*fn);
        la.runOnFunction(spec_fn, *fGraph);

      } else {
        la.runOnFunction(*fn, *fGraph);
      }

      graphs[fn] = fGraph;
    }

    std::vector<CallGraphNode *> cgns = call_graph_utils::SortedCGNs(scc);
    for (CallGraphNode *cgn : cgns) {
      Function *fn = cgn->getFunction();
      if (!fn) continue;

      // -- resolve all function calls in the SCC
      auto callRecords =
          call_graph_utils::SortedCallSites(cgn, m_dsaLibFuncInfo);
      for (auto *callRecord : callRecords) {
        DsaCallSite dsaCS(*dyn_cast<llvm::CallBase>(callRecord));
        const Function *callee = dsaCS.getCallee();
        if (!callee) continue;
        if (callee->isDeclaration() && !m_dsaLibFuncInfo.hasSpecFunc(*callee))
          continue;

        assert(graphs.count(dsaCS.getCaller()) > 0);
        assert(graphs.count(dsaCS.getCallee()) > 0);

        Graph &callerG = *(graphs.find(dsaCS.getCaller())->second);
        Graph &calleeG = *(graphs.find(dsaCS.getCallee())->second);

        static int cnt = 0;
        ++cnt;
        LOG("dsa-bu", llvm::errs() << "BU #" << cnt << ": "
                                   << dsaCS.getCaller()->getName() << " <- "
                                   << dsaCS.getCallee()->getName() << "\n");
        LOG("dsa-bu", llvm::errs()
                          << "\tCallee size: " << calleeG.numNodes()
                          << ", caller size:\t" << callerG.numNodes() << "\n");
        LOG("dsa-bu", llvm::errs()
                          << "\tCallee collapsed: " << calleeG.numCollapsed()
                          << ", caller collapsed:\t" << callerG.numCollapsed()
                          << "\n");

        cloneAndResolveArguments(dsaCS, calleeG, callerG, m_dsaLibFuncInfo,
                                 m_flowSensitiveOpt && !NoBUFlowSensitiveOpt);

        LOG("dsa-bu", llvm::errs()
                          << "\tCaller size after clone: " << callerG.numNodes()
                          << ", collapsed: " << callerG.numCollapsed() << "\n");
      }
    }

    if (fGraph) fGraph->compress();
  }

  if (m_dsaLibFuncInfo.genSpecs()) {
    for (auto &KVP : graphs) {
      m_dsaLibFuncInfo.generateSpec(*KVP.first, KVP.second);
    }
    m_dsaLibFuncInfo.writeSpecModule();
  }

  LOG(
      "dsa-bu-graph", for (auto &kv
                           : graphs) {
        errs() << "### Bottom-up Dsa graph for " << kv.first->getName() << "\n";
        kv.second->write(errs());
        errs() << "\n";
      });

  LOG("dsa-bu", errs() << "Finished bottom-up analysis\n");
  return false;
}

} // namespace seadsa
