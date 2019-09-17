#include "llvm/ADT/DenseMap.h"
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
#include "sea_dsa/CallGraphUtils.hh"
#include "sea_dsa/CallSite.hh"
#include "sea_dsa/Cloner.hh"
#include "sea_dsa/CompleteCallGraph.hh"
#include "sea_dsa/Graph.hh"
#include "sea_dsa/Local.hh"
#include "sea_dsa/config.h"
#include "sea_dsa/support/Debug.h"

#include <unordered_set>
#include <vector>

using namespace llvm;

namespace sea_dsa {
extern bool NoBUFlowSensitiveOpt;
}

namespace sea_dsa {

// XXX: already defined in DsaBottomUp.cc
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

static Value *stripBitCast(Value *V) {
  if (BitCastInst *BC = dyn_cast<BitCastInst>(V)) {
    return BC->getOperand(0);
  } else {
    return V;
  }
}

// XXX: similar to Graph::import but with two modifications:
// - the callee graph is modified during the cloning of call sites.
// - call sites are copied.
void CompleteCallGraphAnalysis::mergeGraphs(Graph &fromG, Graph &toG) {
  Cloner C(toG, CloningContext::mkNoContext(), Cloner::Options::Basic);
  for (auto &kv : fromG.globals()) {
    // -- clone node
    Node &n = C.clone(*kv.second->getNode());
    // -- re-create the cell
    Cell c(n, kv.second->getRawOffset());
    // -- insert value
    Cell &nc = toG.mkCell(*kv.first, Cell());
    // -- unify the old and new cells
    nc.unify(c);
  }

  for (auto &kv : fromG.formals()) {
    Node &n = C.clone(*kv.second->getNode());
    Cell c(n, kv.second->getRawOffset());
    Cell &nc = toG.mkCell(*kv.first, Cell());
    nc.unify(c);
  }
  for (auto &kv : fromG.returns()) {
    Node &n = C.clone(*kv.second->getNode());
    Cell c(n, kv.second->getRawOffset());
    Cell &nc = toG.mkRetCell(*kv.first, Cell());
    nc.unify(c);
  }

  cloneCallSites(C, fromG, toG);
  // possibly created many indirect links, compress
  toG.compress();
}

// Copy callsites from callee to caller graph using the cloner.
void CompleteCallGraphAnalysis::cloneCallSites(Cloner &C, Graph &calleeG,
                                               Graph &callerG) {
  for (DsaCallSite &calleeCS : calleeG.callsites()) {
    const Instruction &I = *(calleeCS.getInstruction());
    if (calleeCS.hasCell()) { // should always have a cell
      const Cell &c = calleeCS.getCell();
      // Ask the cloner if the called cell was cloned
      if (C.hasNode(*c.getNode())) {
        // -- Mark the callee's callsite as cloned:
        //
        // We keep track whether the node of the callee's cell has
        // been copied one into a caller.  The mark is kept in the
        // callee graph. Upon completion of a bottom-up iteration, we
        // use this mark to decide whether or not the callsite can be
        // resolved, and then, we reset it before we start the next
        // bottom-up iteration.
        calleeCS.markCloned();
        // -- Get the cloned node nc
        Cell nc(C.at(*c.getNode()), c.getRawOffset());
        // -- Clone the callsite:
        // if the callsite already exists in the caller then we unify
        // the existing callee's cell with nc, otherwise we create a
        // new callsite with nc.
        if (DsaCallSite *callerCS = callerG.getCallSite(I)) {
          Cell &calledC = callerCS->getCell();
          calledC.unify(nc);
        } else {
          callerG.mkCallSite(I, nc);
        }
      } else {
        if (&calleeG == &callerG) {
          // we pretend the callsite has been cloned.
          calleeCS.markCloned();
        }
      }
    }
  }
}

// Clone callee nodes into caller and resolve arguments
// XXX: already defined in DsaBottomUp but it clones call sites.
void CompleteCallGraphAnalysis::cloneAndResolveArgumentsAndCallSites(
    const DsaCallSite &CS, Graph &calleeG, Graph &callerG, bool noescape) {
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
    }
  }

  // clone callsites
  cloneCallSites(C, calleeG, callerG);

  callerG.compress();
}

CompleteCallGraphAnalysis::CompleteCallGraphAnalysis(
    const llvm::DataLayout &dl, const llvm::TargetLibraryInfo &tli,
    const AllocWrapInfo &allocInfo, llvm::CallGraph &cg, bool noescape)
    : m_dl(dl), m_tli(tli), m_allocInfo(allocInfo), m_cg(cg),
      m_complete_cg(new CallGraph(m_cg.getModule())), m_noescape(noescape) {}

bool CompleteCallGraphAnalysis::runOnModule(Module &M, GraphMap &graphs) {

  typedef std::unordered_set<const Function *> FunctionSet;
  typedef std::unordered_set<const Instruction *> InstSet;

  LOG("dsa-callgraph",
      errs() << "Started construction of complete call graph ... \n");

  const bool track_callsites = true;
  LocalAnalysis la(m_dl, m_tli, m_allocInfo, track_callsites);

  // Given a callsite, inline the callee's graph into the caller's graph.
  auto inlineCallee = [&graphs, this](DsaCallSite &dsaCS, unsigned numIter,
                                      FunctionSet &inlinedFunctions, int &cnt) {
    assert(dsaCS.getCallee());
    assert(graphs.count(dsaCS.getCaller()) > 0);
    assert(graphs.count(dsaCS.getCallee()) > 0);

    Graph &callerG = *(graphs.find(dsaCS.getCaller())->second);
    Graph &calleeG = *(graphs.find(dsaCS.getCallee())->second);

    inlinedFunctions.insert(dsaCS.getCallee());

    ++cnt;

    LOG("dsa-callgraph-bu",
        llvm::errs() << "BU #" << cnt << " iteration=" << numIter << " : "
                     << dsaCS.getCaller()->getName() << " <- "
                     << dsaCS.getCallee()->getName() << "\n");
    LOG("dsa-callgraph-bu",
        llvm::errs() << "\tCallee size: " << calleeG.numNodes()
                     << ", caller size:\t" << callerG.numNodes() << "\n");
    LOG("dsa-callgraph-bu",
        llvm::errs() << "\tCallee collapsed: " << calleeG.numCollapsed()
                     << ", caller collapsed:\t" << callerG.numCollapsed()
                     << "\n");

    cloneAndResolveArgumentsAndCallSites(dsaCS, calleeG, callerG, m_noescape);
    LOG("dsa-callgraph-bu",
        llvm::errs() << "\tCaller size after clone: " << callerG.numNodes()
                     << ", collapsed: " << callerG.numCollapsed() << "\n");
  };

  /// To keep track if there is a change in the callgraph
  bool change = true;
  unsigned numIter = 1;
  /// set of indirect calls that cannot be fully resolved.
  InstSet IncompleteCallSites;
  /// To query if a function has been inlined during the bottom-up traversal.
  FunctionSet InlinedFunctions;
  while (change) {
    InlinedFunctions.clear();
    IncompleteCallSites.clear();

    /// This loop performs a bottom-up traversal while inlining
    /// callee's graphs into callers. The callgraph is augmented with
    /// new edges after each iteration.
    for (auto it = scc_begin(&*m_complete_cg); !it.isAtEnd(); ++it) {
      auto &scc = *it;
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

        if (numIter == 1) {
          la.runOnFunction(*fn, *fGraph);
          graphs[fn] = fGraph;
        } else {
          // After the first bottom-up iteration, a new cycle in the
          // callgraph can be created so we merge all SCC's graphs
          if (fGraph != graphs[fn]) {
            // XXX: we don't use import because it does not copy call sites.
            // fGraph->import(*(graphs[fn]), true);

            mergeGraphs(*(graphs[fn]), *fGraph);
            graphs[fn] = fGraph;
          }
        }
      }
      std::vector<CallGraphNode *> cgns = call_graph_utils::SortedCGNs(scc);
      for (CallGraphNode *cgn : cgns) {
        Function *fn = cgn->getFunction();
        if (!fn || fn->isDeclaration() || fn->empty())
          continue;

        static int cnt = 0;
        // Inline direct and indirect calls
        for (auto &callRecord : *cgn) {
          // Deal with things like this:
          // call void (...) bitcast (void ()* @parse_dir_colors to void
          // (...)*)()
          Value *calledV = stripBitCast(callRecord.first);
          CallSite CS(calledV);
          if (CS.isIndirectCall()) {
            // indirect call:
            //
            // the indirect call might have been already resolved so
            // we ask the call graph for a possible callee.
            const Function *callee = callRecord.second->getFunction();
            if (!callee || callee->isDeclaration() || callee->empty()) {
              continue;
            }
            DsaCallSite dsaCS(*CS.getInstruction(), *callee);
            inlineCallee(dsaCS, numIter, InlinedFunctions, cnt);
          } else {
            DsaCallSite dsaCS(*CS.getInstruction());
            const Function *callee = dsaCS.getCallee();
            // XXX: callee can be still nullptr if the callee is asm
            if (!callee || callee->isDeclaration() || callee->empty()) {
              continue;
            }
            // direct call
            inlineCallee(dsaCS, numIter, InlinedFunctions, cnt);
          }
        }
      }

      if (fGraph)
        fGraph->compress();
    }
    //// end bottom-up phase

    /// Add new edges in the call graph.
    ///
    /// An original indirect callsite CS can be copied into both
    /// callers' and into their ancestors' graphs during the bottom-up
    /// phase. At a given graph G with a copy of an indirect call
    /// (copyCS) we identify some of the possible callees of CS if
    /// the callee's cell (calleeC) in copyCS was never cloned.
    ///
    /// We extract the possible callees from the allocation sites
    /// associated to calleeC's node. Thus, in order to fully resolve
    /// an indirect call we need to be able to perform the
    /// above-mentioned process for all copies of the original
    /// indirect call. If, for instance, one of the allocation sites
    /// is an external call we won't be able to fully resolve the
    /// indirect call.

    // Return true iff there is a callgraph edge between src and dst
    // with callsite CS.
    auto hasEdge = [](CallGraphNode *src, CallGraphNode *dst, CallSite &CS) {
      return std::any_of(
          src->begin(), src->end(),
          [&dst, &CS](typename CallGraphNode::CallRecord &record) {
            return (record.second == dst &&
                    record.first == CS.getInstruction());
          });
    };

    change = false;
    for (auto &kv : graphs) {
      for (DsaCallSite &cs : kv.second->callsites()) {
        if (kv.first->getName().equals("main") ||
	    // JN: a callsite might not be cloned if the function has
	    // not been inlined yet.  I think it's safe to relax the
	    // condition of being inlined.
	    (!cs.isCloned() /*&&
			      InlinedFunctions.count(kv.first) > 0*/)) {

          // Get all possible callees from the allocation sites
          auto &alloc_sites = cs.getCell().getNode()->getAllocSites();
          if (alloc_sites.empty()) {
#if 0
	    errs() << "WARNING: callsite at " << cs.getCaller()->getName()
	     	   << " in graph " << kv.first->getName()
	     	   << " without allocation site at iteration " << numIter << "\n"
	     	   << "\t" << *cs.getInstruction() << "\n"
	     	   << "\t" << *(cs.getCell().getNode()) << "\n";
#endif
            // IncompleteCallSites.insert(cs.getInstruction());
            continue;
          }

          const Function *caller = cs.getCaller();
          CallGraphNode *CGNCaller = (*m_complete_cg)[caller];
          assert(CGNCaller);
          CallSite CGNCS(const_cast<Instruction *>(cs.getInstruction()));

          LOG("dsa-callgraph-resolve", errs() << "Resolved indirect call by "
                                              << kv.first->getName() << " at ";
              errs() << cs.getInstruction()->getParent()->getParent()->getName()
                     << ":\n";
              cs.write(errs()); errs() << "\n";);

          // Update the callgraph by adding a new edge to each
          // resolved callee.
          for (const Value *v : alloc_sites) {
            if (const Function *fn = dyn_cast<const Function>(v)) {
              CallGraphNode *CGNCallee = (*m_complete_cg)[fn];
              assert(CGNCallee);
              if (!hasEdge(CGNCaller, CGNCallee, CGNCS)) {
                CGNCaller->addCalledFunction(CGNCS, CGNCallee);
                change = true;
              }
            } else {
              // If external call then we shouldn't remove the
              // original edge with the indirect call.
              IncompleteCallSites.insert(cs.getInstruction());
              LOG("dsa-callgraph-resolve", errs()
                                               << "Marked as incomplete.\n";);
            }
          }
        }

        // reset the cloned flag for next bottom-up iteration

        // XXX: we don't clear the cloned mark here because two
        // functions can share the same graph, so if we clear it here
        // the next function will think the callsite was not cloned.

        // cs.markCloned(false);
      }
    }

    // FIXME: avoid traverse again callsites.
    for (auto &kv : graphs) {
      for (DsaCallSite &cs : kv.second->callsites()) {
        cs.markCloned(false);
      }
    }

    ++numIter;
  }

  // XXX: compilation error if moved inside LOG
  typedef DenseMap<Instruction *, FunctionVector> callsite_to_callees_map_t;
  LOG(
      "dsa-callgraph-stats", unsigned num_ind_calls = 0;
      unsigned num_resolved_calls = 0; unsigned num_asm_calls = 0;
      callsite_to_callees_map_t callee_map;

      for (auto &F
           : M) {
        if (CallGraphNode *CGNF = (*m_complete_cg)[&F]) {
          for (auto &kv : llvm::make_range(CGNF->begin(), CGNF->end())) {
            if (!kv.first)
              continue;
            CallSite CS(kv.first);
            Instruction *I = CS.getInstruction();
            if (CS.isIndirectCall()) {
              auto &callees = callee_map[I];
              if (!kv.second->getFunction()) {
                // original indirect call
                ++num_ind_calls;
              } else {
                // resolved indirect call
                if (const Function *calleeF = kv.second->getFunction()) {
                  callees.push_back(calleeF);
                }
              }
            } else if (CS.isInlineAsm()) {
              ++num_asm_calls;
            }
          }
        }
      }

      unsigned id = 0;
      for (auto &kv
           : callee_map) {
        assert(kv.first);
        errs() << "#" << ++id;
        if (!kv.second.empty()) {
          if (IncompleteCallSites.find(kv.first) == IncompleteCallSites.end()) {
            ++num_resolved_calls;
            errs() << " COMPLETE ";
          } else {
            errs() << " INCOMPLETE ";
          }
        } else {
          errs() << " INCOMPLETE ";
        }
        errs() << *kv.first;
        errs() << "   callees={";
        for (unsigned i = 0, num_callees = kv.second.size(); i < num_callees;) {
          auto calleeF = kv.second[i];
          auto calleeTy = calleeF->getFunctionType();
          errs() << *(calleeTy->getReturnType());
          errs() << " " << calleeF->getName() << "(";
          for (unsigned j = 0, num_params = calleeTy->getNumParams();
               j < num_params;) {
            errs() << *(calleeTy->getParamType(j));
            ++j;
            if (j < num_params) {
              errs() << ",";
            }
          }
          errs() << ")";
          ++i;
          if (i < num_callees) {
            errs() << ",";
          }
        }
        errs() << "}\n";
      }

      errs()
      << "\nSea-Dsa CallGraph statistics\n";
      errs() << "** Total number of indirect calls " << num_ind_calls << "\n";
      if (num_asm_calls > 0) errs()
      << "** Total number of asm calls " << num_asm_calls << "\n";
      errs() << "** Total number of resolved indirect calls "
             << num_resolved_calls << "\n";);

  /// Remove edges in the callgraph: remove original indirect call
  /// from call graph if we now for sure we fully resolved it.
  for (auto &F : M) {
    if (CallGraphNode *CGNF = (*m_complete_cg)[&F]) {
      // collect first callsites to avoid invalidating CGNF's
      // iterators
      std::vector<CallSite> toRemove;
      for (auto &kv : llvm::make_range(CGNF->begin(), CGNF->end())) {
        if (!kv.first)
          continue;
        CallSite CS(kv.first);
        if (CS.isIndirectCall() &&
            !kv.second->getFunction() && /* external node: no callee*/
            IncompleteCallSites.find(CS.getInstruction()) ==
                IncompleteCallSites.end()) {
          toRemove.push_back(CS);
        }
      }
      for (CallSite CS : toRemove) {
        CGNF->removeCallEdgeFor(CS);
      }
    }
  }

  // Clear up callsites because they are not needed anymore
  // They are only used to build the complete call graph.
  for (auto &kv : graphs) {
    kv.second->clearCallSites();
  }

  LOG(
      "dsa-callgraph-bu-graph", for (auto &kv
                                     : graphs) {
        errs() << "### Bottom-up Dsa graph for " << kv.first->getName() << "\n";
        kv.second->write(errs());
        errs() << "\n";
      });

  LOG("dsa-callgraph-show",
      // errs() << "Original call graph\n";
      // m_cg.print(errs());
      errs() << "Complete call graph built by sea-dsa\n";
      m_complete_cg->print(errs()););

  LOG("dsa-callgraph", errs()
                           << "Finished construction of complete call graph\n");
  return false;
}

CallGraph &CompleteCallGraphAnalysis::getCompleteCallGraphRef() {
  assert(m_complete_cg);
  return *m_complete_cg;
}

const CallGraph &CompleteCallGraphAnalysis::getCompleteCallGraphRef() const {
  assert(m_complete_cg);
  return *m_complete_cg;
}

std::unique_ptr<CallGraph> CompleteCallGraphAnalysis::getCompleteCallGraph() {
  return std::move(m_complete_cg);
}

CompleteCallGraph::CompleteCallGraph()
    : ModulePass(ID), m_dl(nullptr), m_tli(nullptr), m_complete_cg(nullptr) {}

void CompleteCallGraph::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<TargetLibraryInfoWrapperPass>();
  AU.addRequired<CallGraphWrapperPass>();
  AU.addRequired<AllocWrapInfo>();
  AU.setPreservesAll();
}

bool CompleteCallGraph::runOnModule(Module &M) {
  m_dl = &M.getDataLayout();
  m_tli = &getAnalysis<TargetLibraryInfoWrapperPass>().getTLI();
  m_allocInfo = &getAnalysis<AllocWrapInfo>();
  CallGraph &cg = getAnalysis<CallGraphWrapperPass>().getCallGraph();

  CompleteCallGraphAnalysis ccga(*m_dl, *m_tli, *m_allocInfo, cg, true);
  for (auto &F : M) { // XXX: the graphs must be created here
    if (F.isDeclaration() || F.empty())
      continue;
    GraphRef fGraph = std::make_shared<Graph>(*m_dl, m_setFactory);
    m_graphs[&F] = fGraph;
  }
  bool res = ccga.runOnModule(M, m_graphs);
  m_complete_cg = std::move(ccga.getCompleteCallGraph());
  return res;
}

CallGraph &CompleteCallGraph::getCompleteCallGraph() {
  assert(m_complete_cg);
  return *m_complete_cg;
}

const CallGraph &CompleteCallGraph::getCompleteCallGraph() const {
  assert(m_complete_cg);
  return *m_complete_cg;
}

char CompleteCallGraph::ID = 0;
} // namespace sea_dsa

static llvm::RegisterPass<sea_dsa::CompleteCallGraph>
    X("seadsa-complete-callgraph", "Construct SeaDsa call graph pass");
