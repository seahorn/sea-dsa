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
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

#include "sea_dsa/BottomUp.hh"
#include "sea_dsa/CallGraph.hh"
#include "sea_dsa/CallSite.hh"
#include "sea_dsa/Cloner.hh"
#include "sea_dsa/Global.hh"
#include "sea_dsa/Graph.hh"
#include "sea_dsa/Local.hh"
#include "sea_dsa/config.h"

// #include "ufo/Stats.hh"
#include "sea_dsa/support/Debug.h"

#include "boost/range/iterator_range.hpp"

#include <queue>

static llvm::cl::opt<bool> normalizeUniqueScalars(
    "sea-dsa-norm-unique-scalar",
    llvm::cl::desc("DSA: all callees and callers agree on unique scalars"),
    llvm::cl::init(true), llvm::cl::Hidden);

static llvm::cl::opt<bool> normalizeAllocaSites(
    "sea-dsa-norm-alloca-sites",
    llvm::cl::desc("DSA: all callees and callers agree on allocation sites"),
    llvm::cl::init(true), llvm::cl::Hidden);

using namespace llvm;

/// CONTEXT-INSENSITIVE DSA
namespace sea_dsa {

void ContextInsensitiveGlobalAnalysis::resolveArguments(DsaCallSite &cs,
                                                        Graph &g) {
  // unify return
  const Function &callee = *cs.getCallee();
  if (g.hasRetCell(callee)) {
    Cell &nc = g.mkCell(*cs.getInstruction(), Cell());
    const Cell &r = g.getRetCell(callee);
    Cell c(*r.getNode(), r.getRawOffset(), FieldType::NotImplemented());
    nc.unify(c);
  }

  // unify actuals and formals
  DsaCallSite::const_actual_iterator AI = cs.actual_begin(),
                                     AE = cs.actual_end();
  for (DsaCallSite::const_formal_iterator FI = cs.formal_begin(),
                                          FE = cs.formal_end();
       FI != FE && AI != AE; ++FI, ++AI) {
    const Value *arg = (*AI).get();
    const Value *farg = &*FI;
    if (g.hasCell(*farg)) {
      Cell &c = g.mkCell(*arg, Cell());
      Cell &d = g.mkCell(*farg, Cell());
      c.unify(d);
    }
  }
}

bool ContextInsensitiveGlobalAnalysis::runOnModule(Module &M) {

  LOG("dsa-global",
      errs() << "Started context-insensitive global analysis ... \n");

  // ufo::Stats::resume ("CI-DsaAnalysis");

  if (kind() == FLAT_MEMORY)
    m_graph.reset(new FlatGraph(m_dl, m_setFactory));
  else
    m_graph.reset(new Graph(m_dl, m_setFactory));

  LocalAnalysis la(m_dl, m_tli);

  // -- bottom-up inlining of all graphs
  for (auto it = scc_begin(&m_cg); !it.isAtEnd(); ++it) {
    auto &scc = *it;

    // --- all scc members share the same local graph
    for (CallGraphNode *cgn : scc) {
      Function *fn = cgn->getFunction();
      if (!fn || fn->isDeclaration() || fn->empty())
        continue;

      // compute local graph
      GraphRef fGraph = nullptr;
      if (kind() == FLAT_MEMORY)
        fGraph.reset(new FlatGraph(m_dl, m_setFactory));
      else
        fGraph.reset(new Graph(m_dl, m_setFactory));

      la.runOnFunction(*fn, *fGraph);

      m_fns.insert(fn);
      m_graph->import(*fGraph, true);
    }

    // --- resolve callsites
    for (CallGraphNode *cgn : scc) {
      Function *fn = cgn->getFunction();

      // XXX probably not needed since if the function is external
      // XXX it will have no call records
      if (!fn || fn->isDeclaration() || fn->empty())
        continue;

      // -- iterate over all call instructions of the current function fn
      // -- they are indexed in the CallGraphNode data structure
      for (auto &CallRecord : *cgn) {
        ImmutableCallSite cs(CallRecord.first);
        DsaCallSite dsa_cs(cs);
        const Function *callee = dsa_cs.getCallee();
        // XXX We want to resolve external calls as well.
        // XXX By not resolving them, we pretend that they have no
        // XXX side-effects. This should be an option, not the only behavior
        if (callee && !callee->isDeclaration() && !callee->empty()) {
          assert(fn == dsa_cs.getCaller());
          resolveArguments(dsa_cs, *m_graph);
        }
      }
    }
    m_graph->compress();
  }
  m_graph->remove_dead();

  // ufo::Stats::stop ("CI-DsaAnalysis");

  LOG("dsa-global-graph", errs() << "### Global Dsa graph \n";
      m_graph->write(errs()); errs() << "\n");

  LOG("dsa-global",
      errs() << "Finished context-insensitive global analysis.\n");

  return false;
}

const Graph &
ContextInsensitiveGlobalAnalysis::getGraph(const Function &) const {
  assert(m_graph);
  return *m_graph;
}

Graph &ContextInsensitiveGlobalAnalysis::getGraph(const Function &) {
  assert(m_graph);
  return *m_graph;
}

bool ContextInsensitiveGlobalAnalysis::hasGraph(const Function &fn) const {
  return m_fns.count(&fn) > 0;
}

/// LLVM passes

ContextInsensitiveGlobal::ContextInsensitiveGlobal()
    : DsaGlobalPass(ID), m_ga(nullptr) {}

void ContextInsensitiveGlobal::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<TargetLibraryInfoWrapperPass>();
  AU.addRequired<CallGraphWrapperPass>();
  AU.setPreservesAll();
}

bool ContextInsensitiveGlobal::runOnModule(Module &M) {
  auto &dl = M.getDataLayout();
  auto &tli = getAnalysis<TargetLibraryInfoWrapperPass>().getTLI();
  auto &cg = getAnalysis<CallGraphWrapperPass>().getCallGraph();

  const bool useFlatMemory = false;
  m_ga.reset(new ContextInsensitiveGlobalAnalysis(dl, tli, cg, m_setFactory,
                                                  useFlatMemory));
  return m_ga->runOnModule(M);
}

FlatMemoryGlobal::FlatMemoryGlobal() : DsaGlobalPass(ID), m_ga(nullptr) {}

void FlatMemoryGlobal::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<TargetLibraryInfoWrapperPass>();
  AU.addRequired<CallGraphWrapperPass>();
  AU.setPreservesAll();
}

bool FlatMemoryGlobal::runOnModule(Module &M) {
  auto &dl = M.getDataLayout();
  auto &tli = getAnalysis<TargetLibraryInfoWrapperPass>().getTLI();
  auto &cg = getAnalysis<CallGraphWrapperPass>().getCallGraph();

  const bool useFlatMemory = true;
  m_ga.reset(new ContextInsensitiveGlobalAnalysis(dl, tli, cg, m_setFactory,
                                                  useFlatMemory));
  return m_ga->runOnModule(M);
}

} // end namespace sea_dsa

/// CONTEXT-SENSITIVE DSA
namespace sea_dsa {

// A simple worklist implementation
template <typename T> struct WorkList<T>::impl {
  std::queue<T> m_w;
  std::set<T> m_s;
};
template <typename T>
WorkList<T>::WorkList() : m_pimpl(new WorkList<T>::impl()) {}
template <typename T> bool WorkList<T>::empty() const {
  return m_pimpl->m_w.empty();
}
template <typename T> size_t WorkList<T>::size() const {
  return m_pimpl->m_w.size();
}
template <typename T> void WorkList<T>::enqueue(const T &e) {
  auto p = m_pimpl->m_s.insert(e);
  if (p.second) {
    m_pimpl->m_w.push(e);
  }
}
template <typename T> const T &WorkList<T>::dequeue() {
  const T &e = m_pimpl->m_w.front();
  m_pimpl->m_w.pop();
  m_pimpl->m_s.erase(e);
  return e;
}

// Clone caller nodes into callee and resolve arguments
// XXX: this code is pretty much symmetric to the one defined in
// BottomUp. They should be merged at some point.
void ContextSensitiveGlobalAnalysis::cloneAndResolveArguments(
    const DsaCallSite &cs, Graph &callerG, Graph &calleeG) {

  Cloner C(calleeG);

  // clone and unify globals
  for (auto &kv : boost::make_iterator_range(callerG.globals_begin(),
                                             callerG.globals_end())) {
    Node &n = C.clone(*kv.second->getNode());
    Cell c(n, kv.second->getRawOffset(), kv.second->getType());
    Cell &nc = calleeG.mkCell(*kv.first, Cell());
    nc.unify(c);
  }

  // clone and unify return
  const Function &callee = *cs.getCallee();
  if (calleeG.hasRetCell(callee) && callerG.hasCell(*cs.getInstruction())) {
    auto &inst = *cs.getInstruction();
    const Cell &csCell = callerG.getCell(inst);
    Node &n = C.clone(*csCell.getNode());
    Cell c(n, csCell.getRawOffset(), csCell.getType());
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
      Node &n = C.clone(*callerCell.getNode());
      Cell c(n, callerCell.getRawOffset(), callerCell.getType());
      Cell &nc = calleeG.mkCell(*fml, Cell());
      nc.unify(c);
    }
  }
  calleeG.compress();
}

// Decide which kind of propagation (if any) is needed
ContextSensitiveGlobalAnalysis::PropagationKind
ContextSensitiveGlobalAnalysis::decidePropagation(const DsaCallSite &cs,
                                                  Graph &calleeG,
                                                  Graph &callerG) {

  PropagationKind res = UP;
  SimulationMapper sm;
  if (Graph::computeCalleeCallerMapping(cs, calleeG, callerG, sm, false)) {
    if (sm.isFunction()) {
      // isInjective only checks modified nodes by default
      res = (sm.isInjective() ? NONE : DOWN);
    }
  }
  return res;
}

void ContextSensitiveGlobalAnalysis::propagateTopDown(const DsaCallSite &cs,
                                                      Graph &callerG,
                                                      Graph &calleeG) {
  cloneAndResolveArguments(cs, callerG, calleeG);

  LOG("dsa-global", if (decidePropagation(cs, calleeG, callerG) == DOWN) {
    errs() << "Sanity check failed:"
           << " we should not need more top-down propagation\n";
  });
  // errs () << "Top-down propagation at " << *cs.getInstruction () << "\n";
  assert(decidePropagation(cs, calleeG, callerG) != DOWN);
}

void ContextSensitiveGlobalAnalysis::propagateBottomUp(const DsaCallSite &cs,
                                                       Graph &calleeG,
                                                       Graph &callerG) {
  BottomUpAnalysis::cloneAndResolveArguments(cs, calleeG, callerG);

  LOG("dsa-global", if (decidePropagation(cs, calleeG, callerG) == UP) {
    errs() << "Sanity check failed:"
           << " we should not need more bottom-up propagation\n";
  });
  // errs () << "Bottom-up propagation at " << *cs.getInstruction () << "\n";
  assert(decidePropagation(cs, calleeG, callerG) != UP);
}

bool ContextSensitiveGlobalAnalysis::runOnModule(Module &M) {

  LOG("dsa-global",
      errs() << "Started context-sensitive global analysis ... \n");

  // ufo::Stats::resume ("CS-DsaAnalysis");

  for (auto &F : M) {
    if (F.isDeclaration() || F.empty())
      continue;

    GraphRef fGraph = std::make_shared<Graph>(m_dl, m_setFactory);
    m_graphs[&F] = fGraph;
  }

  // -- Run bottom up analysis on the whole call graph
  //    and initialize worklist
  BottomUpAnalysis bu(m_dl, m_tli, m_cg);
  bu.runOnModule(M, m_graphs);

  DsaCallGraph dsaCG(m_cg);
  dsaCG.buildDependencies();

  WorkList<const Instruction *> w;

  /// push in the worklist callsites for which two different
  /// callee nodes are mapped to the same caller node
  for (auto &kv : boost::make_iterator_range(bu.callee_caller_mapping_begin(),
                                             bu.callee_caller_mapping_end())) {
    auto const &simMapper = *(kv.second);
    assert(simMapper.isFunction());

    if (!simMapper.isInjective())
      w.enqueue(kv.first); // they do need top-down
  }

  /// -- top-down/bottom-up propagation until no change

  LOG("dsa-global",
      errs () << "Initially " << w.size () << " callsite to propagate\n";);
  
  unsigned td_props = 0;
  unsigned bu_props = 0;
  while (!w.empty()) {
    const Instruction *I = w.dequeue();

    if (const CallInst *CI = dyn_cast<CallInst>(I))
      if (CI->isInlineAsm())
        continue;

    ImmutableCallSite CS(I);
    DsaCallSite dsaCS(CS);

    auto callee = dsaCS.getCallee();
    if (!callee || callee->isDeclaration() || callee->empty())
      continue;

    LOG("dsa-global",
	errs() << "Selected callsite " << *I << " from queue ... ";);
    auto caller = dsaCS.getCaller();

    assert(m_graphs.count(caller) > 0);
    assert(m_graphs.count(callee) > 0);

    Graph &callerG = *(m_graphs.find(caller)->second);
    Graph &calleeG = *(m_graphs.find(callee)->second);

    // -- find out which propagation is needed if any
    auto propKind = decidePropagation(dsaCS, calleeG, callerG);
    if (propKind == DOWN) {
      propagateTopDown(dsaCS, callerG, calleeG);
      td_props++;
      auto &calleeU = dsaCG.getUses(*callee);
      auto &calleeD = dsaCG.getDefs(*callee);
      for (auto ci : calleeU)
        w.enqueue(ci); // they might need bottom-up
      for (auto ci : calleeD)
        w.enqueue(ci); // they might need top-down
    } else if (propKind == UP) {
      propagateBottomUp(dsaCS, calleeG, callerG);
      bu_props++;
      auto &callerU = dsaCG.getUses(*caller);
      auto &callerD = dsaCG.getDefs(*caller);
      for (auto ci : callerU)
        w.enqueue(ci); // they might need bottom-up
      for (auto ci : callerD)
        w.enqueue(ci); // they might need top-down
    }
    LOG("dsa-global", errs() << "processed\n";);
  }

  LOG("dsa-global",
      errs() << "-- Number of top-down propagations=" << td_props << "\n";
      errs() << "-- Number of bottom-up propagations=" << bu_props << "\n";);

#ifdef SANITY_CHECKS
  assert(checkNoMorePropagation());
#endif

  /// FIXME: propagate both in the same fixpoint
  if (normalizeUniqueScalars) {
    CallGraphClosure<ContextSensitiveGlobalAnalysis, UniqueScalar> usa(*this,
                                                                       dsaCG);
    usa.runOnModule(M);
  }

  if (normalizeAllocaSites) {
    CallGraphClosure<ContextSensitiveGlobalAnalysis, AllocaSite> asa(*this,
                                                                     dsaCG);
    asa.runOnModule(M);
  }

  // Removing dead nodes (if any)
  for (auto &kv : m_graphs)
    kv.second->remove_dead();

  LOG("dsa-global-graph", for (auto &kv
                               : m_graphs) {
    errs() << "### Global Dsa graph for " << kv.first->getName() << "\n";
    kv.second->write(errs());
    errs() << "\n";
  });

  LOG("dsa-global", errs() << "Finished context-sensitive global analysis\n");

  // ufo::Stats::stop ("CS-DsaAnalysis");

  return false;
}

// Perform some sanity checks:
// 1) each callee node can be simulated by its corresponding caller node.
// 2) no two callee nodes are mapped to the same caller node.
bool ContextSensitiveGlobalAnalysis::checkNoMorePropagation() {
  for (auto it = scc_begin(&m_cg); !it.isAtEnd(); ++it) {
    auto &scc = *it;
    for (CallGraphNode *cgn : scc) {
      Function *fn = cgn->getFunction();
      if (!fn || fn->isDeclaration() || fn->empty())
        continue;

      for (auto &callRecord : *cgn) {
        ImmutableCallSite CS(callRecord.first);
        DsaCallSite cs(CS);

        const Function *callee = cs.getCallee();
        if (!callee || callee->isDeclaration() || callee->empty())
          continue;

        assert(m_graphs.count(cs.getCaller()) > 0);
        assert(m_graphs.count(cs.getCallee()) > 0);

        Graph &callerG = *(m_graphs.find(cs.getCaller())->second);
        Graph &calleeG = *(m_graphs.find(cs.getCallee())->second);
        PropagationKind pkind = decidePropagation(cs, calleeG, callerG);
        if (pkind != NONE) {
          auto pkind_str = (pkind == UP) ? "bottom-up" : "top-down";
          errs() << "ERROR sanity check failed:" << *(cs.getInstruction())
                 << " requires " << pkind_str << " propagation.\n";
          return false;
        }
      }
    }
  }
  errs() << "Sanity check succeed: global propagation completed!\n";
  return true;
}

const Graph &
ContextSensitiveGlobalAnalysis::getGraph(const Function &fn) const {
  return *(m_graphs.find(&fn)->second);
}

Graph &ContextSensitiveGlobalAnalysis::getGraph(const Function &fn) {
  return *(m_graphs.find(&fn)->second);
}

bool ContextSensitiveGlobalAnalysis::hasGraph(const Function &fn) const {
  return m_graphs.count(&fn) > 0;
}

/// LLVM pass

ContextSensitiveGlobal::ContextSensitiveGlobal()
    : DsaGlobalPass(ID), m_ga(nullptr) {
  // initializeCallGraphWrapperPassPass(*PassRegistry::getPassRegistry());
}

void ContextSensitiveGlobal::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<TargetLibraryInfoWrapperPass>();
  AU.addRequired<CallGraphWrapperPass>();
  AU.setPreservesAll();
}

bool ContextSensitiveGlobal::runOnModule(Module &M) {
  auto &dl = M.getDataLayout();
  auto &tli = getAnalysis<TargetLibraryInfoWrapperPass>().getTLI();
  auto &cg = getAnalysis<CallGraphWrapperPass>().getCallGraph();

  m_ga.reset(new ContextSensitiveGlobalAnalysis(dl, tli, cg, m_setFactory));
  return m_ga->runOnModule(M);
}
} // namespace sea_dsa

namespace sea_dsa {

// propagate unique scalars across callsites
void UniqueScalar::runOnCallSite(const DsaCallSite &cs, Node &calleeN,
                                 Node &callerN) {
  unsigned changed = calleeN.mergeUniqueScalar(callerN);
  if (changed & 0x01) // calleeN changed
  {
    if (const Function *fn = cs.getCallee()) {
      for (auto ci : m_dsaCG.getUses(*fn))
        m_w.enqueue(ci);
      for (auto ci : m_dsaCG.getDefs(*fn))
        m_w.enqueue(ci);
    }
  }

  if (changed & 0x02) // callerN changed
  {
    if (const Function *fn = cs.getCaller()) {
      for (auto ci : m_dsaCG.getUses(*fn))
        m_w.enqueue(ci);
      for (auto ci : m_dsaCG.getDefs(*fn))
        m_w.enqueue(ci);
    }
  }
}

// propagate allocation sites across callsites
void AllocaSite::runOnCallSite(const DsaCallSite &cs, Node &calleeN,
                               Node &callerN) {
  unsigned changed = calleeN.mergeAllocSites(callerN);
  if (changed & 0x01) // calleeN changed
  {
    if (const Function *fn = cs.getCallee()) {
      for (auto ci : m_dsaCG.getUses(*fn))
        m_w.enqueue(ci);
      for (auto ci : m_dsaCG.getDefs(*fn))
        m_w.enqueue(ci);
    }
  }

  if (changed & 0x02) // callerN changed
  {
    if (const Function *fn = cs.getCaller()) {
      for (auto ci : m_dsaCG.getUses(*fn))
        m_w.enqueue(ci);
      for (auto ci : m_dsaCG.getDefs(*fn))
        m_w.enqueue(ci);
    }
  }
}

// Quick closure implementation over a call graph's callsites
template <class GA, class Op>
bool CallGraphClosure<GA, Op>::runOnModule(Module &M) {
  for (auto it = scc_begin(&m_dsaCG.getCallGraph()); !it.isAtEnd(); ++it) {
    auto &scc = *it;
    for (CallGraphNode *cgn : scc) {
      Function *fn = cgn->getFunction();
      if (!fn || fn->isDeclaration() || fn->empty())
        continue;

      for (auto &callRecord : *cgn) {
        ImmutableCallSite CS(callRecord.first);
        DsaCallSite dsaCS(CS);

        const Function *callee = dsaCS.getCallee();
        if (!callee || callee->isDeclaration() || callee->empty())
          continue;

        if (m_ga.hasGraph(*dsaCS.getCaller()) &&
            m_ga.hasGraph(*dsaCS.getCallee())) {
          Graph &calleeG = m_ga.getGraph(*dsaCS.getCallee());
          Graph &callerG = m_ga.getGraph(*dsaCS.getCaller());
          exec_callsite(dsaCS, calleeG, callerG);
        }
      }
    }
  }

  while (!m_w.empty()) {
    const Instruction *I = m_w.dequeue();
    ImmutableCallSite CS(I);
    DsaCallSite dsaCS(CS);

    if (dsaCS.getCaller() && m_ga.hasGraph(*dsaCS.getCaller()) &&
        dsaCS.getCallee() && m_ga.hasGraph(*dsaCS.getCallee())) {
      Graph &calleeG = m_ga.getGraph(*dsaCS.getCallee());
      Graph &callerG = m_ga.getGraph(*dsaCS.getCaller());
      exec_callsite(dsaCS, calleeG, callerG);
    }
  }
  return false;
}

template <class GA, class Op>
void CallGraphClosure<GA, Op>::exec_callsite(const DsaCallSite &cs,
                                             Graph &calleeG, Graph &callerG) {
  // globals
  for (auto &kv : boost::make_iterator_range(calleeG.globals_begin(),
                                             calleeG.globals_end())) {
    Cell &c = *kv.second;
    Cell &nc = callerG.mkCell(*kv.first, Cell());
    Op op(m_dsaCG, m_w);
    op.runOnCallSite(cs, *c.getNode(), *nc.getNode());
  }

  // return
  const Function &callee = *cs.getCallee();
  if (calleeG.hasRetCell(callee) && callerG.hasCell(*cs.getInstruction())) {
    Cell &c = calleeG.getRetCell(callee);
    Cell &nc = callerG.mkCell(*cs.getInstruction(), Cell());
    Op op(m_dsaCG, m_w);
    op.runOnCallSite(cs, *c.getNode(), *nc.getNode());
  }

  // actuals and formals
  DsaCallSite::const_actual_iterator AI = cs.actual_begin(),
                                     AE = cs.actual_end();
  for (DsaCallSite::const_formal_iterator FI = cs.formal_begin(),
                                          FE = cs.formal_end();
       FI != FE && AI != AE; ++FI, ++AI) {
    const Value *arg = (*AI).get();
    const Value *fml = &*FI;
    if (callerG.hasCell(*arg) && calleeG.hasCell(*fml)) {
      Cell &c = calleeG.mkCell(*fml, Cell());
      Cell &nc = callerG.mkCell(*arg, Cell());
      Op op(m_dsaCG, m_w);
      op.runOnCallSite(cs, *c.getNode(), *nc.getNode());
    }
  }
}
} // namespace sea_dsa

char sea_dsa::FlatMemoryGlobal::ID = 0;

char sea_dsa::ContextInsensitiveGlobal::ID = 0;

char sea_dsa::ContextSensitiveGlobal::ID = 0;

static llvm::RegisterPass<sea_dsa::FlatMemoryGlobal>
    X("seadsa-flat-global", "Flat memory Dsa analysis");

static llvm::RegisterPass<sea_dsa::ContextInsensitiveGlobal>
    Y("seadsa-ci-global", "Context-insensitive Dsa analysis");

static llvm::RegisterPass<sea_dsa::ContextSensitiveGlobal>
    Z("seadsa-cs-global", "Context-sensitive Dsa analysis");
