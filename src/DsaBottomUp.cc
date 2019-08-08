#include "sea_dsa/BottomUp.hh"

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
#include "sea_dsa/support/Brunch.hh"
#include "sea_dsa/support/Debug.h"

using namespace llvm;

static llvm::cl::opt<bool> NoBUFlowSensitiveOpt(
    "sea-dsa-no-bu-flow-sensitive-opt",
    llvm::cl::desc("Disable partial flow sensitivity in bottom up"),
    llvm::cl::init(false), llvm::cl::Hidden);

namespace sea_dsa {

static const Value *findUniqueReturnValue(const Function &F) {
  const Value *onlyRetVal = nullptr;

  for (const auto &BB : F) {
    auto *TI = BB.getTerminator();
    auto *RI = dyn_cast<ReturnInst>(TI);
    if (!RI)
      continue;

    const Value *rv = RI->getOperand(0)->stripPointerCastsNoFollowAliases();
    if (onlyRetVal && onlyRetVal != rv)
      return nullptr;

    onlyRetVal = rv;
  }

  return onlyRetVal;
}

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
static void reachableNodes(const Function &fn, Graph &g, Set &inputReach,
                           Set &retReach) {
  // formal parameters
  for (const Value &arg : fn.args()) {
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

// Copy (recursively) all callsites owned by calleeC to graph callerG.
// Precondition: callerC simulates calleeC.
static void copyCallSites(Graph &calleeG, const Cell &calleeC, Graph &callerG,
                          Cell &callerC, std::set<Node *> &visited,
                          std::set<DsaCallSite *> &copiedCallSites) {

  // break cycles
  if (!visited.insert(calleeC.getNode()).second) {
    return;
  }

  // Copy callsite from current cell
  Node *calleeN = calleeC.getNode();
  Node *callerN = callerC.getNode();
  for (const Value *cs : calleeN->getCallSites()) {
    llvm::Optional<DsaCallSite *> DsaCs = calleeG.getCallSite(*cs);
    if (!DsaCs.hasValue())
      continue;

    assert(DsaCs.getValue()->hasCell() &&
           "The indirect callsite should have a cell");

    copiedCallSites.insert(DsaCs.getValue());

    llvm::Optional<DsaCallSite *> callerCS =
        callerG.getCallSite(*(DsaCs.getValue()->getInstruction()));
    if (callerCS.hasValue()) {
      assert(callerCS.getValue()->hasCell());
      callerCS.getValue()->getCell().unify(callerC);
    } else {
      callerG.mkCallSite(*(DsaCs.getValue()->getInstruction()), callerC);
    }
  }

  // Copy recursively callsites from reachable cells
  for (auto &kv : calleeN->links()) {
    if (kv.second->isNull())
      continue;

    Cell &succCalleeC = *kv.second;
    Field j = kv.first;
    assert(callerN->hasLink(j));
    Cell &succCallerC = callerN->getLink(j);
    copyCallSites(calleeG, succCalleeC, callerG, succCallerC, visited,
                  copiedCallSites);
  }
}

static void copyCallSites(Graph &calleeG, const Cell &calleeC, Graph &callerG,
                          Cell &callerC,
                          std::set<DsaCallSite *> &copiedCallSites) {
  std::set<Node *> visited;
  copyCallSites(calleeG, calleeC, callerG, callerC, visited, copiedCallSites);
}

// Clone callee nodes into caller and resolve arguments
void BottomUpAnalysis::cloneAndResolveArguments(const DsaCallSite &CS,
                                                Graph &calleeG, Graph &callerG,
                                                bool noescape) {
  std::set<DsaCallSite *> copiedCallSites /*unused*/;
  cloneAndResolveArguments(CS, calleeG, callerG, noescape, copiedCallSites);
}

// Clone callee nodes into caller and resolve arguments
void BottomUpAnalysis::cloneAndResolveArguments(
    const DsaCallSite &CS, Graph &calleeG, Graph &callerG, bool noescape,
    std::set<DsaCallSite *> &copiedCallSites) {
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
    if (!NoBUFlowSensitiveOpt)
      if (calleeN.getNumLinks() == 0 || !calleeN.isModified() ||
          llvm::isa<ConstantData>(kv.first))
        continue;
#endif

    const Value &global = *kv.first;
    Node &n = C.clone(calleeN, false, kv.first);
    Cell c(n, kv.second->getRawOffset());
    Cell &nc = callerG.mkCell(*kv.first, Cell());
    nc.unify(c);

    // copy call sites owned by the callee node into the caller graph
    copyCallSites(calleeG, *kv.second, callerG, nc, copiedCallSites);
  }

  // clone and unify return
  const Function &callee = *CS.getCallee();
  if (calleeG.hasRetCell(callee)) {
    Cell &nc = callerG.mkCell(*CS.getInstruction(), Cell());

    // Clone the return value directly, if we know that it corresponds to a
    // single allocation site (e.g., return value of a malloc, a global, etv.).
    const Value *onlyAllocSite = findUniqueReturnValue(callee);
    if (onlyAllocSite && !calleeG.hasAllocSiteForValue(*onlyAllocSite))
      onlyAllocSite = nullptr;
    if (NoBUFlowSensitiveOpt)
      onlyAllocSite = nullptr;

    const Cell &ret = calleeG.getRetCell(callee);
    Node &n = C.clone(*ret.getNode(), false, onlyAllocSite);
    Cell c(n, ret.getRawOffset());
    nc.unify(c);

    // copy call sites owned by the callee node into the caller graph
    copyCallSites(calleeG, ret, callerG, nc, copiedCallSites);
  }

  // clone and unify actuals and formals
  DsaCallSite::const_actual_iterator AI = CS.actual_begin(),
                                     AE = CS.actual_end();
  for (DsaCallSite::const_formal_iterator FI = CS.formal_begin(),
                                          FE = CS.formal_end();
       FI != FE && AI != AE; ++FI, ++AI) {
    const Value *arg = (*AI).get();
    const Value *fml = &*FI;
    if (calleeG.hasCell(*fml)) {
      const Cell &formalC = calleeG.getCell(*fml);
      Node &n = C.clone(*formalC.getNode());
      Cell c(n, formalC.getRawOffset());
      Cell &nc = callerG.mkCell(*arg, Cell());
      nc.unify(c);

      // copy call sites owned by the callee node into the caller graph
      copyCallSites(calleeG, formalC, callerG, nc, copiedCallSites);
    }
  }

  callerG.compress();
}

bool BottomUpAnalysis::checkAllNodesAreMapped(const Function &fn, Graph &fnG,
                                              const SimulationMapper &sm) {

  std::set<const Node *> reach;
  std::set<const Node *> retReach /*unused*/;
  reachableNodes(fn, fnG, reach, retReach);
  for (const Node *n : reach) {

    Cell callerC = sm.get(Cell(const_cast<Node *>(n), 0));
    if (callerC.isNull()) {
      errs() << "ERROR: callee node " << *n
             << " not mapped to a caller node.\n";
      return false;
    }
  }
  return true;
}

template <typename T> std::vector<CallGraphNode *> SortedCGNs(const T &t) {
  std::vector<CallGraphNode *> cgns;
  for (CallGraphNode *cgn : t) {
    Function *fn = cgn->getFunction();
    if (!fn || fn->isDeclaration() || fn->empty())
      continue;

    cgns.push_back(cgn);
  }

  std::stable_sort(
      cgns.begin(), cgns.end(), [](CallGraphNode *first, CallGraphNode *snd) {
        return first->getFunction()->getName() < snd->getFunction()->getName();
      });

  return cgns;
}

std::vector<const llvm::Value *> SortedCallSites(CallGraphNode *cgn) {
  std::vector<const llvm::Value *> res;
  res.reserve(cgn->size());

  for (auto &callRecord : *cgn) {
    ImmutableCallSite CS(callRecord.first);
    DsaCallSite dsaCS(CS);
    const Function *callee = dsaCS.getCallee();
    if (!callee || callee->isDeclaration() || callee->empty())
      continue;

    res.push_back(callRecord.first);
  }

  std::stable_sort(res.begin(), res.end(),
                   [](const Value *first, const Value *snd) {
                     ImmutableCallSite CS1(first);
                     DsaCallSite dsaCS1(CS1);
                     StringRef callerN1 = dsaCS1.getCaller()->getName();
                     StringRef calleeN1 = dsaCS1.getCallee()->getName();

                     ImmutableCallSite CS2(snd);
                     DsaCallSite dsaCS2(CS2);
                     StringRef callerN2 = dsaCS2.getCaller()->getName();
                     StringRef calleeN2 = dsaCS2.getCallee()->getName();

                     return std::make_pair(callerN1, calleeN1) <
                            std::make_pair(callerN2, calleeN2);
                   });

  return res;
}

bool BottomUpAnalysis::runOnModule(Module &M, GraphMap &graphs) {

  LOG("dsa-bu", errs() << "Started bottom-up analysis ... \n");

// Keep it true until implementation is stable
#ifndef SANITY_CHECKS
  const bool do_sanity_checks = true;
#else
  const bool do_sanity_checks = true;
#endif

  BrunchTimer buTime("BU_AND_LOCAL");
  BrunchTimer localTime("LOCAL");
  localTime.pause();

  LocalAnalysis la(m_dl, m_tli, m_allocInfo);

  const size_t totalFunctions = M.getFunctionList().size();
  size_t functionsProcessed = 0;
  size_t percentageProcessed = 0;

  /// -- keep track of indirect call sites that have been copied to a
  /// -- caller. This helps us to decide whether or not an indirect
  /// -- callsite can be resolved: given function F, if a callsite C
  /// -- is not copied to F's callers and F' address is never taken,
  /// -- then the callsite C can be resolved.
  std::set<DsaCallSite *> copiedCallSites;
  for (auto it = scc_begin(&m_cg); !it.isAtEnd(); ++it) {
    auto &scc = *it;

    functionsProcessed += scc.size();
    const size_t oldProgress = percentageProcessed;
    percentageProcessed = 100 * functionsProcessed / totalFunctions;
    if (percentageProcessed != oldProgress)
      SEA_DSA_BRUNCH_PROGRESS("BU_FUNCTIONS_PROCESSED_PERCENT",
                              percentageProcessed, 100ul);

    // -- compute a local graph shared between all functions in the scc
    GraphRef fGraph = nullptr;
    for (CallGraphNode *cgn : scc) {
      Function *fn = cgn->getFunction();
      if (!fn || fn->isDeclaration() || fn->empty())
        continue;

      if (!fGraph) {
        assert(graphs.find(fn) != graphs.end());
        fGraph = graphs[fn];
        assert(fGraph);
      }

      localTime.resume();
      la.runOnFunction(*fn, *fGraph);
      graphs[fn] = fGraph;
      localTime.pause();
    }

    std::vector<CallGraphNode *> cgns = SortedCGNs(scc);
    for (CallGraphNode *cgn : cgns) {
      Function *fn = cgn->getFunction();
      if (!fn || fn->isDeclaration() || fn->empty())
        continue;

      // -- resolve all *direct* function calls in the SCC
      auto callRecords = SortedCallSites(cgn);
      for (auto *callRecord : callRecords) {
        ImmutableCallSite CS(callRecord);
        DsaCallSite dsaCS(CS);
        const Function *callee = dsaCS.getCallee();
        if (!callee || callee->isDeclaration() || callee->empty()) {
          continue;
        }

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

        cloneAndResolveArguments(dsaCS, calleeG, callerG, m_noescape,
                                 copiedCallSites);
        LOG("dsa-bu", llvm::errs()
                          << "\tCaller size after clone: " << callerG.numNodes()
                          << ", collapsed: " << callerG.numCollapsed() << "\n");
      }

      /////
      /// FIXME: Commenting this code breaks the SAS'17 code.
      /////
      // if (m_computeSimMap) {
      //   // -- store the simulation maps from the SCC
      //   for (auto &callRecord : *cgn) {
      //     ImmutableCallSite CS(callRecord.first);
      //     DsaCallSite dsaCS(CS);
      //     const Function *callee = dsaCS.getCallee();
      //     if (!callee || callee->isDeclaration() || callee->empty())
      //       continue;

      //     assert(graphs.count(dsaCS.getCaller()) > 0);
      //     assert(graphs.count(dsaCS.getCallee()) > 0);

      //     Graph &callerG = *(graphs.find(dsaCS.getCaller())->second);
      //     Graph &calleeG = *(graphs.find(dsaCS.getCallee())->second);

      //     SimulationMapperRef sm(new SimulationMapper());
      //     bool res = Graph::computeCalleeCallerMapping(dsaCS, calleeG,
      //     callerG,
      //                                                  *sm,
      //                                                  do_sanity_checks);
      //     if (!res) {
      //       // continue;
      //       llvm_unreachable("Simulation mapping check failed");
      //     }
      //     m_callee_caller_map.insert(
      //         std::make_pair(dsaCS.getInstruction(), sm));

      //     if (do_sanity_checks) {
      //       // Check the simulation map is a function
      //       if (!sm->isFunction())
      //         errs() << "ERROR: simulation map for " <<
      //         *dsaCS.getInstruction()
      //                << " is not a function!\n";
      //       // Check that all nodes in the callee are mapped to one
      //       // node in the caller graph
      //       checkAllNodesAreMapped(*callee, calleeG, *sm);
      //     }
      //   }
      // }
    }

    if (fGraph)
      fGraph->compress();
  }

  /// Compute address-taken functions
  /// FIXME: improve performance by maybe doing it together with the
  ///        above loop that inlines graphs.
  ///
  /// TODO: this set should be refined by removing functions that have
  ///       been resolved.
  std::set<const Function *> addressTakenFunctions;
  for (auto &kv : graphs) {
    for (DsaAllocSite &as : kv.second->alloc_sites()) {
      if (const Function *asF = dyn_cast<const Function>(&(as.getValue()))) {
        addressTakenFunctions.insert(asF);
      }
    }
  }

  /// Mark non-copied callsites as resolved.
  for (auto &kv : graphs) {
    // If the address of a function is taken then it's possible that
    // its callsites are never copied because they are never inlined.
    if (addressTakenFunctions.count(kv.first) <= 0) {
      for (DsaCallSite &dsaCS : kv.second->callsites()) {
        if (copiedCallSites.count(&dsaCS) <= 0) {
          dsaCS.markResolved();
        }
      }
    }
  }

  /// TODO:
  ///
  /// Option A:
  ///   while (true)
  ///     run the bottom-up pass (the local pass only the first iteration)
  ///     if not new resolved callsites return;
  ///     modify the CallGraph to add a new edge from f1 to f2 if there
  ///     is a new resolved callsite in f1 that calls f2.
  ///
  /// When we add a new edge in the callgraph, the edge must be
  /// attached to some call or invoke instruction. We would need to
  /// modify the callgraph for that.
  ///
  ///
  /// Option B:
  ///     1. Modify DsaCallSite API to have a new method getCallees() such
  ///        that if the call site is indirect and resolve then returns all
  ///        possible callees. That will require changes in formal_begin()
  ///        and formal_end() to take a function as parameter.
  ///     2. Modify the bottom-up pass to call
  ///        cloneAndResolveArguments on each possible callee graph.
  ///
  ///     while (true)
  ///        run modified bottom-up pass (the local pass only the first
  ///        iteration) if not new resolved callsites return;

  localTime.stop();
  buTime.stop();

  LOG("dsa-bu-graph", for (auto &kv
                           : graphs) {
    errs() << "### Bottom-up Dsa graph for " << kv.first->getName() << "\n";
    kv.second->write(errs());
    errs() << "\n";
  });

  LOG("dsa-bu", errs() << "Finished bottom-up analysis\n");

  return false;
}

BottomUp::BottomUp() : ModulePass(ID), m_dl(nullptr), m_tli(nullptr) {}

void BottomUp::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<TargetLibraryInfoWrapperPass>();
  AU.addRequired<CallGraphWrapperPass>();
  AU.addRequired<AllocWrapInfo>();
  AU.setPreservesAll();
}

bool BottomUp::runOnModule(Module &M) {
  m_dl = &M.getDataLayout();
  m_tli = &getAnalysis<TargetLibraryInfoWrapperPass>().getTLI();
  m_allocInfo = &getAnalysis<AllocWrapInfo>();
  CallGraph &cg = getAnalysis<CallGraphWrapperPass>().getCallGraph();

  BottomUpAnalysis bu(*m_dl, *m_tli, *m_allocInfo, cg, true /*sim map*/);
  for (auto &F : M) { // XXX: the graphs must be created here
    if (F.isDeclaration() || F.empty())
      continue;

    GraphRef fGraph = std::make_shared<Graph>(*m_dl, m_setFactory);
    m_graphs[&F] = fGraph;
  }

  return bu.runOnModule(M, m_graphs);
}

Graph &BottomUp::getGraph(const Function &F) const {
  return *(m_graphs.find(&F)->second);
}

bool BottomUp::hasGraph(const Function &F) const {
  return m_graphs.count(&F) > 0;
}

char BottomUp::ID = 0;
} // namespace sea_dsa

static llvm::RegisterPass<sea_dsa::BottomUp> X("seadsa-bu",
                                               "Bottom-up DSA pass");
