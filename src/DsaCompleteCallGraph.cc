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
#include "llvm/Support/CommandLine.h"
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
bool PrintCallGraphStats;
extern bool NoBUFlowSensitiveOpt;
}

static llvm::cl::opt<bool, true> XPrintCallGraphStats(
    "sea-dsa-callgraph-stats",
    llvm::cl::desc("Print stats about the SeaDsa call graph"),
    llvm::cl::location(sea_dsa::PrintCallGraphStats),
    llvm::cl::init(false));

static llvm::cl::opt<unsigned> PrintVerbosity(
    "sea-dsa-callgraph-stats-verbose",
    llvm::cl::Hidden,
    llvm::cl::init(1));

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

void CompleteCallGraphAnalysis::printStats(Module& M, raw_ostream& o) {
  unsigned num_total_calls = 0;
  unsigned num_direct_calls = 0;
  unsigned num_indirect_resolved_calls = 0;
  unsigned num_indirect_unresolved_calls = 0;
  unsigned num_asm_calls = 0;
  unsigned num_unexpected_calls = 0;
  
  std::string str;
  raw_string_ostream str_os(str);

  auto printFunction = [&str_os](const Function* F) {
    auto Ty = F->getFunctionType();
    str_os << *(Ty->getReturnType());
    str_os << " " << F->getName() << "(";
    for (unsigned i = 0, num_params = Ty->getNumParams(); i < num_params;) {
      str_os << *(Ty->getParamType(i));
      ++i;
      if (i < num_params) {
	str_os << ",";
      }
    }
    str_os << ")";
  };

  for (auto &F: M) {
    if (F.isDeclaration()) continue;
    for (auto &I: llvm::make_range(inst_begin(&F), inst_end(&F))) {
      if (!(isa<CallInst>(I) || isa<InvokeInst>(I))) continue;

      ++num_total_calls;
      DsaCallSite DsaCS(I);
      CallSite CS(&I);
      if (DsaCS.getCallee()) { // DsaCallSite looks through bitcasts and aliases
	++num_direct_calls;
      } else if (CS.isIndirectCall()) {
	if (isComplete(CS)) {
	  ++num_indirect_resolved_calls;
	  if (PrintVerbosity > 0) {
	    str_os << *CS.getInstruction();
	    if (const DebugLoc dbgloc = CS.getInstruction()->getDebugLoc()) {
	      str_os << " at ";
	      dbgloc.print(str_os);
	    }
	    str_os << " ### RESOLVED\n  Callees:{";
	    for (auto it = begin(CS), et = end(CS); it!=et;) {
	      const Function* calleeF = *it;
	      printFunction(calleeF);
	      ++it;
	      if (it != et){
		str_os << ",";
	      }
	    }
	    str_os << "}\n";
	  }
	} else {
	  ++num_indirect_unresolved_calls;
	  if (PrintVerbosity > 0) {
	    str_os << *CS.getInstruction() << " ";
	    if (const DebugLoc dbgloc = CS.getInstruction()->getDebugLoc()) {
	      str_os << " at ";
	      dbgloc.print(str_os);
	    }
	    str_os << " ### UNRESOLVED\n";
	    // TODO: we can give more information even if the call was not resolved:
	    // - is it partially resolved?
	    // - is marked as external?
	    // - ?
	  }
	}
      } else if (CS.isInlineAsm()) {
	++num_asm_calls;      
      } else {
	++num_unexpected_calls;
	errs() << *CS.getInstruction() << "\n";
	errs() << *CS.getCalledValue() << "\n";
      }
    }
  }

  
  o << "\n=== Sea-Dsa CallGraph Statistics === \n";
  o << "** Total number of total calls " << num_total_calls << "\n";  
  o << "** Total number of direct calls " << num_direct_calls << "\n";
  o << "** Total number of asm calls " << num_asm_calls << "\n";
  o << "** Total number of indirect calls "
    << num_indirect_resolved_calls + num_indirect_unresolved_calls << "\n";
  o << "\t** Total number of indirect calls resolved by Sea-Dsa "
    << num_indirect_resolved_calls << "\n";
  o << "\t** Total number of indirect calls unresolved by Sea-Dsa "
    << num_indirect_unresolved_calls << "\n";
  if (num_unexpected_calls > 0) {
    o << "** Total number of unexpected calls " << num_unexpected_calls << "\n";
  }
  if (PrintVerbosity > 0) {
    o << "\n\n" << str_os.str() << "\n";
  }
}


CompleteCallGraphAnalysis::CompleteCallGraphAnalysis(
    const llvm::DataLayout &dl, const llvm::TargetLibraryInfo &tli,
    const AllocWrapInfo &allocInfo, llvm::CallGraph &cg, bool noescape)
    : m_dl(dl), m_tli(tli), m_allocInfo(allocInfo), m_cg(cg),
      m_complete_cg(new CallGraph(m_cg.getModule())), m_noescape(noescape) {}

bool CompleteCallGraphAnalysis::runOnModule(Module &M) {

  typedef std::unordered_set<const Instruction *> InstSet;

  LOG("dsa-callgraph",
      errs() << "Started construction of complete call graph ... \n");

  GraphMap graphs;

  // initialize empty graphs
  for (auto &F : M) { 
    if (F.isDeclaration() || F.empty())
      continue;
    GraphRef fGraph = std::make_shared<Graph>(m_dl, m_setFactory);
    graphs[&F] = fGraph;
  }

  const bool track_callsites = true;  
  LocalAnalysis la(m_dl, m_tli, m_allocInfo, track_callsites);

  // Given a callsite, inline the callee's graph into the caller's graph.
  auto inlineCallee = [&graphs, this](DsaCallSite &dsaCS, unsigned numIter, int &cnt) {
    assert(dsaCS.getCallee());
    assert(graphs.count(dsaCS.getCaller()) > 0);
    assert(graphs.count(dsaCS.getCallee()) > 0);

    Graph &callerG = *(graphs.find(dsaCS.getCaller())->second);
    Graph &calleeG = *(graphs.find(dsaCS.getCallee())->second);

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
  while (change) {
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
            inlineCallee(dsaCS, numIter, cnt);
          } else {
            DsaCallSite dsaCS(*CS.getInstruction());
            const Function *callee = dsaCS.getCallee();
            // XXX: callee can be still nullptr if the callee is asm
            if (!callee || callee->isDeclaration() || callee->empty()) {
              continue;
            }
            // direct call
            inlineCallee(dsaCS, numIter, cnt);
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
	    // XXX: a callsite might not be cloned if the function has
	    // not been inlined yet but in that case its allocation
	    // sites should be empty and we skip the callsite.
	    (!cs.isCloned())) {

          // Get all possible callees from the allocation sites
          auto &alloc_sites = cs.getCell().getNode()->getAllocSites();
          if (alloc_sites.empty()) {
#if 0
	    // This can happen in early fixpoint iterations or if
	    // some callsite is in a function that is never inlined
	    // (e.g., if its address is taken and passed to an
	    // external call that is supposed to call it).
	    errs() << "WARNING: callsite at " << cs.getCaller()->getName()
	     	   << " in graph " << kv.first->getName()
	     	   << " without allocation site at iteration " << numIter << "\n"
	     	   << "\t" << *cs.getInstruction() << "\n"
	     	   << "\t" << *(cs.getCell().getNode()) << "\n";
#endif
            continue;
          }

          const Function *caller = cs.getCaller();
          CallGraphNode *CGNCaller = (*m_complete_cg)[caller];
          assert(CGNCaller);
          CallSite CGNCS(const_cast<Instruction *>(cs.getInstruction()));

	  /// At this point, we can try to resolve the indirect call
	 
          // Update the callgraph by adding a new edge to each
          // resolved callee.
	  bool has_external_alloc_site = false;
          for (const Value *v : alloc_sites) {
            if (const Function *fn = dyn_cast<const Function>(v)) {
              CallGraphNode *CGNCallee = (*m_complete_cg)[fn];
              assert(CGNCallee);
              if (!hasEdge(CGNCaller, CGNCallee, CGNCS)) {
                CGNCaller->addCalledFunction(CGNCS, CGNCallee);
		m_callees[cs.getInstruction()].push_back(fn);
                change = true;
              }
            } else {
              // If external call then we shouldn't remove the
              // original edge with the indirect call.
              IncompleteCallSites.insert(cs.getInstruction());
	      has_external_alloc_site = true;
	      LOG("dsa-callgraph-resolve",
		  errs() << "Resolving indirect call by "
		         << kv.first->getName() << " at "
		         << cs.getInstruction()->getParent()->getParent()->getName()
                         << ":\n";
		  cs.write(errs()); errs() << "\n";
		  errs() << "Marked as incomplete.\n";);
            }
          }
	  
	  if (!has_external_alloc_site) {
	    // At this point we know about the indirect call that :
	    // 1) it can be resolved. This happens either because:
	    //    - we have already reached main during the bottom-up process or
	    //    - the callsite cannot be inlined anymore.
	    // 2) it has at least one allocation site (i.e., some
	    //    callee to resolve it)
	    // 3) it has no external allocation site.
	    m_resolved.insert(cs.getInstruction());
	    // LOG("dsa-callgraph-resolve",
	    // 	errs() << "Resolving indirect call by "
	    // 	       << kv.first->getName() << " at "
	    // 	       << cs.getInstruction()->getParent()->getParent()->getName()
            //            << ":\n";
	    // 	cs.write(errs()); errs() << "\n";
	    // 	errs() << "Marked as complete.\n";);
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
            !kv.second->getFunction() /* has no callee */) {
	  if (m_resolved.count(CS.getInstruction()) > 0)
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

bool CompleteCallGraphAnalysis::isComplete(CallSite& CS) const {
  return m_resolved.count(CS.getInstruction()) > 0;
}

CompleteCallGraphAnalysis::callee_iterator
CompleteCallGraphAnalysis::begin(llvm::CallSite& CS) {
  return m_callees[CS.getInstruction()].begin();
}

CompleteCallGraphAnalysis::callee_iterator
CompleteCallGraphAnalysis::end(llvm::CallSite& CS) {
  return m_callees[CS.getInstruction()].end();
}
  
CompleteCallGraph::CompleteCallGraph(bool printStats)
  : ModulePass(ID), m_CCGA(nullptr), m_printStats(printStats) {}

void CompleteCallGraph::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<TargetLibraryInfoWrapperPass>();
  AU.addRequired<CallGraphWrapperPass>();
  AU.addRequired<AllocWrapInfo>();
  AU.setPreservesAll();
}

bool CompleteCallGraph::runOnModule(Module &M) {
  auto dl = &M.getDataLayout();
  auto tli = &getAnalysis<TargetLibraryInfoWrapperPass>().getTLI();
  auto allocInfo = &getAnalysis<AllocWrapInfo>();
  CallGraph &cg = getAnalysis<CallGraphWrapperPass>().getCallGraph();
  m_CCGA.reset(new CompleteCallGraphAnalysis(*dl, *tli, *allocInfo, cg, true));
  m_CCGA->runOnModule(M);
  if (PrintCallGraphStats || m_printStats) {
    m_CCGA->printStats(M, errs());
  }
  return false;
}

CallGraph &CompleteCallGraph::getCompleteCallGraph() {
  return m_CCGA->getCompleteCallGraphRef();
}

const CallGraph &CompleteCallGraph::getCompleteCallGraph() const {
  return m_CCGA->getCompleteCallGraphRef();  
}

bool CompleteCallGraph::isComplete(CallSite& CS) const {
  return m_CCGA->isComplete(CS);
}

CompleteCallGraph::callee_iterator CompleteCallGraph::begin(llvm::CallSite& CS) {
  return m_CCGA->begin(CS);
}

CompleteCallGraph::callee_iterator CompleteCallGraph::end(llvm::CallSite& CS) {
  return m_CCGA->end(CS);  
}
  
char CompleteCallGraph::ID = 0;

Pass *createDsaPrintCallGraphStatsPass() {
  return new CompleteCallGraph(true);
}

} // namespace sea_dsa

static llvm::RegisterPass<sea_dsa::CompleteCallGraph>
    X("seadsa-complete-callgraph", "Construct SeaDsa call graph pass");
