#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SCCIterator.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/CallPromotionUtils.h"

#include "seadsa/AllocWrapInfo.hh"
#include "seadsa/CallGraphUtils.hh"
#include "seadsa/CallSite.hh"
#include "seadsa/Cloner.hh"
#include "seadsa/CompleteCallGraph.hh"
#include "seadsa/Graph.hh"
#include "seadsa/InitializePasses.hh"
#include "seadsa/Local.hh"
#include "seadsa/config.h"
#include "seadsa/support/Debug.h"

#include <unordered_set>
#include <vector>

using namespace llvm;

namespace seadsa {
bool PrintCallGraphStats;
extern bool NoBUFlowSensitiveOpt;
} // namespace seadsa

static llvm::cl::opt<bool, true> XPrintCallGraphStats(
    "sea-dsa-callgraph-stats",
    llvm::cl::desc("Print stats about the SeaDsa call graph"),
    llvm::cl::location(seadsa::PrintCallGraphStats), llvm::cl::init(false));

static llvm::cl::opt<unsigned> PrintVerbosity("sea-dsa-callgraph-stats-verbose",
                                              llvm::cl::Hidden,
                                              llvm::cl::init(1));

namespace seadsa {

// XXX: already defined in DsaBottomUp.cc
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

static void resolveIndirectCallsThroughBitCast(Function &F, CallGraph &seaCg) {
  // Resolve trivial indirect calls through bitcasts:
  //    call void (...) bitcast (void ()* @parse_dir_colors to void (...)*)()
  //    call i32 bitcast (i32 (...)* @nd_uint to i32 ()*)()
  //    call i32 (i32, i32)* bitcast (i32 (i32, i32)* (...)* @nd_binfptr to i32 (i32, i32)* ()*)()
  //
  // This is important because our top-down/bottom-up analyses
  // traverse the call graph in a particular order (topological or
  // reverse topological). If these edges are missing then the
  // propagation can be done "too early" without analyzing the caller
  // or callee yet.
  auto stripBitCast = [](Value *V) {
    if (BitCastInst *BC = dyn_cast<BitCastInst>(V)) {
      return BC->getOperand(0);
    } else {
      return V->stripPointerCasts();
    }
  };
  
  for (auto &I : llvm::make_range(inst_begin(&F), inst_end(&F))) {
    if (!(isa<CallInst>(I) || isa<InvokeInst>(I))) continue;
    CallBase &CB = *dyn_cast<CallBase>(&I);
    Value *calleeV = CB.getCalledOperand();
    if (calleeV != stripBitCast(calleeV)) {
      if (Function *calleeF = dyn_cast<Function>(stripBitCast(calleeV))) {
        CallGraphNode *callerCGN = seaCg[&F];
        CallGraphNode *calleeCGN = seaCg[calleeF];
        callerCGN->removeCallEdgeFor(CB);
        callerCGN->addCalledFunction(&CB, calleeCGN);
        LOG("dsa-callgraph", llvm::errs()
	    << "Added edge from " << F.getName()
	    << " to " << calleeF->getName()
	    << " with callsite=" << CB << "\n";);
      }
    }
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
    // single allocation site (e.g., return value of a malloc, a global, etc.)
    const Value *onlyAllocSite = findUniqueReturnValue(callee);
    if (NoBUFlowSensitiveOpt ||
        (onlyAllocSite && !isa<GlobalValue>(onlyAllocSite))) {
      onlyAllocSite = nullptr;
    }

    const Cell &ret = calleeG.getRetCell(callee);
    Node &n = C.clone(*ret.getNode(), false, onlyAllocSite);
    Cell c(n, ret.getRawOffset());
    nc.unify(c);

    if (onlyAllocSite) {
      assert(isa<llvm::GlobalValue>(onlyAllocSite));
      Cell &nc = callerG.mkCell(*onlyAllocSite, Cell());
      nc.unify(c);
    }
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

  // XX: It is very expensive to compress the caller graph after each
  // callsite is processed. Instead, we compress the caller graph only
  // once after all callsites have been inlined.
  // callerG.compress();
}

void CompleteCallGraphAnalysis::printStats(Module &M, raw_ostream &o) {
  unsigned num_total_calls = 0;
  unsigned num_direct_calls = 0;
  unsigned num_indirect_resolved_calls = 0;
  unsigned num_indirect_unresolved_calls = 0;
  unsigned num_asm_calls = 0;
  unsigned num_unexpected_calls = 0;

  std::string str;
  raw_string_ostream str_os(str);

  auto printFunction = [&str_os](const Function *F) {
    auto Ty = F->getFunctionType();
    str_os << *(Ty->getReturnType());
    str_os << " " << F->getName() << "(";
    for (unsigned i = 0, num_params = Ty->getNumParams(); i < num_params;) {
      str_os << *(Ty->getParamType(i));
      ++i;
      if (i < num_params) { str_os << ","; }
    }
    str_os << ")";
  };

  for (auto &F : M) {
    if (F.isDeclaration()) continue;
    for (auto &I : llvm::make_range(inst_begin(&F), inst_end(&F))) {
      if (!(isa<CallInst>(I) || isa<InvokeInst>(I))) continue;

      ++num_total_calls;
      DsaCallSite DsaCS(I);
      CallBase &CB = *dyn_cast<CallBase>(&I);
      if (DsaCS.getCallee()) { // DsaCallSite looks through bitcasts and aliases
        ++num_direct_calls;
      } else if (CB.isIndirectCall()) {
        if (isComplete(CB)) {
          ++num_indirect_resolved_calls;
          if (PrintVerbosity > 0) {
            str_os << CB; 
            if (const DebugLoc dbgloc = CB.getDebugLoc()) {
              str_os << " at ";
              dbgloc.print(str_os);
            }
            str_os << " ### RESOLVED\n  Callees:{";
            for (auto it = begin(CB), et = end(CB); it != et;) {
              const Function *calleeF = *it;
              printFunction(calleeF);
              ++it;
              if (it != et) { str_os << ","; }
            }
            str_os << "}\n";
          }
        } else {
          ++num_indirect_unresolved_calls;
          if (PrintVerbosity > 0) {
            str_os << CB << " ";
            if (const DebugLoc dbgloc = CB.getDebugLoc()) {
              str_os << " at ";
              dbgloc.print(str_os);
            }
            str_os << " ### UNRESOLVED\n";
            // TODO: we can give more information even if the call was not
            // resolved:
            // - is it partially resolved?
            // - is marked as external?
            // - ?
          }
        }
      } else if (CB.isInlineAsm()) {
        ++num_asm_calls;
      } else {
        ++num_unexpected_calls;
        errs() << CB << "\n";
        errs() << CB.getCalledOperand() << "\n";
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
  if (PrintVerbosity > 0) { o << "\n\n" << str_os.str() << "\n"; }
}

CompleteCallGraphAnalysis::CompleteCallGraphAnalysis(
    const llvm::DataLayout &dl, llvm::TargetLibraryInfoWrapperPass &tliWrapper,
    const AllocWrapInfo &allocInfo, const DsaLibFuncInfo &dsaLibFuncInfo/*unused*/,
    llvm::CallGraph &cg, bool noescape)
    : m_dl(dl), m_tliWrapper(tliWrapper), m_allocInfo(allocInfo),
      m_cg(cg), m_complete_cg(new CallGraph(m_cg.getModule())), m_noescape(noescape) {}

bool CompleteCallGraphAnalysis::runOnModule(Module &M) {

  LOG("dsa-callgraph",
      errs() << "Started construction of complete call graph ... \n");

  GraphMap graphs;

  for (auto &F : M) {
    if (F.isDeclaration() || F.empty()) continue;
    // initialize empty graphs
    GraphRef fGraph = std::make_shared<Graph>(m_dl, m_setFactory);
    graphs[&F] = fGraph;
    // resolve trivial indirect calls
    resolveIndirectCallsThroughBitCast(F, *m_complete_cg);
  }

  const bool track_callsites = true;
  LocalAnalysis la(m_dl, m_tliWrapper, m_allocInfo, track_callsites);

  // Given a callsite, inline the callee's graph into the caller's graph.
  auto inlineCallee = [&graphs, this](DsaCallSite &dsaCS, unsigned numIter,
                                      int &cnt) {
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
  while (change) {
    /// This loop performs a bottom-up traversal while inlining
    /// callee's graphs into callers. The callgraph is augmented with
    /// new edges after each iteration.
    for (auto it = scc_begin(&*m_complete_cg); !it.isAtEnd(); ++it) {
      auto &scc = *it;
      GraphRef fGraph = nullptr;

      for (CallGraphNode *cgn : scc) {
        Function *fn = cgn->getFunction();
        if (!fn || fn->isDeclaration() || fn->empty()) continue;
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
        if (!fn || fn->isDeclaration() || fn->empty()) continue;

        static int cnt = 0;
        // Inline direct and indirect calls
        for (auto &callRecord : *cgn) {
          CallBase &CB = *dyn_cast<CallBase>(*callRecord.first);
          if (CB.isIndirectCall()) {
            // indirect call:
            //
            // the indirect call might have been already resolved so
            // we ask the call graph for a possible callee.
            const Function *callee = callRecord.second->getFunction();
            if (!callee || callee->isDeclaration() || callee->empty()) {
              continue;
            }
            DsaCallSite dsaCS(CB, *callee);
            inlineCallee(dsaCS, numIter, cnt);
          } else {
            DsaCallSite dsaCS(CB);
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

      if (fGraph) fGraph->compress();
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
    auto hasEdge = [](const CallGraphNode *src, const CallGraphNode *dst, const CallBase &CB) {
      return std::any_of(
          src->begin(), src->end(),
          [&dst, &CB](const typename CallGraphNode::CallRecord &record) {
            return (record.second == dst &&
                    *record.first == &CB);
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

          Node *csNode = cs.getCell().getNode();
          // Get all possible callees from the allocation sites
          auto &alloc_sites = csNode->getAllocSites();
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
	  // We remove "const" because of isLegalToPromote
          CallBase *cb = const_cast<CallBase *>(dyn_cast<CallBase>(cs.getInstruction()));
	  
          // At this point, we can try to resolve the indirect call
          // Update the callgraph by adding a new edge to each
          // resolved callee. However, the call site is not marked as
          // fully resolved if the dsa node is marked as external.
          bool foundAtLeastOneCallee = false;
          for (const Value *v : alloc_sites) {
            if (const Function *fn =
                    dyn_cast<Function>(v->stripPointerCastsAndAliases())) {

	      // Check that the callbase and the callee are type-compatible
	      if (!isLegalToPromote(*cb, const_cast<Function*>(fn))) {
	      	continue;
	      }
	      
              foundAtLeastOneCallee = true;
              CallGraphNode *CGNCallee = (*m_complete_cg)[fn];
              assert(CGNCallee);
              if (!hasEdge(CGNCaller, CGNCallee, *cb)) {
                assert(cb);
		LOG("dsa-callgraph",		
		    llvm::errs() << "Added edge from " << caller->getName() << " to "
		    << fn->getName() << " with callsite=" << *cb << "\n";);
                CGNCaller->addCalledFunction(cb, CGNCallee);
                m_callees[cs.getInstruction()].push_back(fn);
                change = true;
              }
            }
          }

          if (foundAtLeastOneCallee && !csNode->isExternal()) {
            // At this point we know about the indirect call that :
            // 1) it can be resolved. This happens either because:
            //    - we have already reached main during the bottom-up process or
            //    - the callsite cannot be inlined anymore.
            // 2) it has at least one allocation site (i.e., some
            //    callee to resolve it)
            // 3) it has no external allocation site.
            m_resolved.insert(cs.getInstruction());
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
  /// from call graph if we know for sure we fully resolved it.
  for (auto &F : M) {
    if (F.empty()) continue;
    
    CallGraphNode *callerCG = (*m_complete_cg)[&F];
    assert(callerCG);
    if (!callerCG) continue;
    
    // This loop removes safely edges while iterating
    auto et = callerCG->end();
    for (auto it = callerCG->begin();it!=et;) {
      if (it->first) {
	CallBase &CB = *dyn_cast<CallBase>(*(it->first));
	if (CB.isIndirectCall() && !it->second->getFunction() /* has no callee */) {
	  if (m_resolved.count(&CB) > 0) {
	    LOG("dsa-callgraph",		
		llvm::errs() << "Removed edge from "
  		<< callerCG->getFunction()->getName() << " to "
		<< it->second << " (external node) with callsite=" << CB << "\n";);
	    
	    // we need to bail out if we are going to remove the last edge
	    bool wasLast = (it+1 == et);
	    // we don't increment it because removeCallEdge modifies *it
	    callerCG->removeCallEdge(it); 
	    if (wasLast) {
	      break;
	    }
	    // we update et after deleting one of the edges
	    et = callerCG->end();
	    continue;
	  }
	}
      }
      ++it;
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

bool CompleteCallGraphAnalysis::isComplete(CallBase &CB) const {
  return m_resolved.count(&CB) > 0;
}




CompleteCallGraphAnalysis::callee_iterator
CompleteCallGraphAnalysis::begin(llvm::CallBase &CB) {
  return m_callees[&CB].begin();
}

CompleteCallGraphAnalysis::callee_iterator
CompleteCallGraphAnalysis::end(llvm::CallBase &CB) {
  return m_callees[&CB].end();
}

CompleteCallGraph::CompleteCallGraph(bool printStats)
    : ModulePass(ID), m_CCGA(nullptr), m_printStats(printStats) {}

void CompleteCallGraph::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<TargetLibraryInfoWrapperPass>();
  AU.addRequired<CallGraphWrapperPass>();
  // dependency for immutable AllowWrapInfo
  AU.addRequired<LoopInfoWrapperPass>();
  AU.addRequired<AllocWrapInfo>();
  AU.addRequired<DsaLibFuncInfo>();
  AU.setPreservesAll();
}

bool CompleteCallGraph::runOnModule(Module &M) {
  auto &dl = M.getDataLayout();
  auto &tli = getAnalysis<TargetLibraryInfoWrapperPass>();
  auto allocInfo = &getAnalysis<AllocWrapInfo>();
  auto dsaLibFuncInfo = &getAnalysis<DsaLibFuncInfo>();
  allocInfo->initialize(M, this);
  dsaLibFuncInfo->initialize(M);
  CallGraph &cg = getAnalysis<CallGraphWrapperPass>().getCallGraph();
  m_CCGA.reset(new CompleteCallGraphAnalysis(dl, tli, *allocInfo,
                                             *dsaLibFuncInfo, cg, true));
  m_CCGA->runOnModule(M);
  if (PrintCallGraphStats || m_printStats) { m_CCGA->printStats(M, errs()); }
  return false;
}

CallGraph &CompleteCallGraph::getCompleteCallGraph() {
  return m_CCGA->getCompleteCallGraphRef();
}

const CallGraph &CompleteCallGraph::getCompleteCallGraph() const {
  return m_CCGA->getCompleteCallGraphRef();
}

bool CompleteCallGraph::isComplete(CallBase &CB) const {
  return m_CCGA->isComplete(CB);
}

CompleteCallGraph::callee_iterator
CompleteCallGraph::begin(llvm::CallBase &CB) {
  return m_CCGA->begin(CB);
}

CompleteCallGraph::callee_iterator CompleteCallGraph::end(llvm::CallBase &CB) {
  return m_CCGA->end(CB);
}

char CompleteCallGraph::ID = 0;

Pass *createDsaPrintCallGraphStatsPass() { return new CompleteCallGraph(true); }

} // namespace seadsa

using namespace seadsa;
INITIALIZE_PASS_BEGIN(CompleteCallGraph, "seadsa-complete-callgraph",
                      "Construct SeaDsa call graph pass", false, false)
INITIALIZE_PASS_DEPENDENCY(LoopInfoWrapperPass)
INITIALIZE_PASS_DEPENDENCY(AllocWrapInfo)
INITIALIZE_PASS_DEPENDENCY(DsaLibFuncInfo)
INITIALIZE_PASS_DEPENDENCY(CallGraphWrapperPass)
INITIALIZE_PASS_DEPENDENCY(TargetLibraryInfoWrapperPass)
INITIALIZE_PASS_END(CompleteCallGraph, "seadsa-complete-callgraph",
                    "Construct SeaDsa call graph pass", false, false)
