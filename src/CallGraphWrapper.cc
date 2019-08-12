#include "llvm/ADT/SCCIterator.h"
#include "llvm/ADT/Optional.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/IR/CallSite.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"

#include "sea_dsa/CallGraphUtils.hh"
#include "sea_dsa/CallGraphWrapper.hh"
#include "sea_dsa/support/Debug.h"

using namespace llvm;

namespace sea_dsa {

void CallGraphWrapper::buildDependencies() {
  // -- Compute immediate callers of each function in the call graph.
  //
  // XXX: CallGraph cannot be reversed and the CallGraph analysis
  // doesn't seem to compute predecessors so I do not know a
  // better way.
  std::unordered_map<const Function *, CallSiteSet> callers;

  auto insertCallers = [&callers](const Function* callee, DsaCallSite CS) {
    auto it = callers.find(callee);
    if (it != callers.end())
      insert(CS, it->second);
    else {
      CallSiteSet s;
      insert(CS, s);
      callers.insert({callee, s});
    }
  };
  
  for (auto it = scc_begin(&m_cg); !it.isAtEnd(); ++it) {
    auto &scc = *it;
    for (CallGraphNode *cgn : scc) {
      const Function *fn = cgn->getFunction();
      if (!fn || fn->isDeclaration() || fn->empty())
        continue;

      for (auto &callRecord : *cgn) {
	llvm::Optional<DsaCallSite> DsaCS =
	  call_graph_utils::getDsaCallSite(callRecord);
	if (!DsaCS.hasValue())
	  continue;
	insertCallers(DsaCS.getValue().getCallee(), DsaCS.getValue());	
      }
    }
  }

  // -- compute uses/defs sets
  for (auto it = scc_begin(&m_cg); !it.isAtEnd(); ++it) {
    auto &scc = *it;

    // compute uses and defs shared between all functions in the scc
    auto uses = std::make_shared<CallSiteSet>();
    auto defs = std::make_shared<CallSiteSet>();

    for (CallGraphNode *cgn : scc) {
      const Function *fn = cgn->getFunction();
      if (!fn || fn->isDeclaration() || fn->empty())
        continue;

      insert(callers[fn].begin(), callers[fn].end(), *uses);

      for (auto &callRecord : *cgn) {
	llvm::Optional<DsaCallSite> DsaCS =
	  call_graph_utils::getDsaCallSite(callRecord);
	if (!DsaCS.hasValue())
	  continue;
        insert(DsaCS.getValue(), *defs);
      }
    }
    
    // store uses and defs
    for (CallGraphNode *cgn : scc) {
      const Function *fn = cgn->getFunction();
      if (!fn || fn->isDeclaration() || fn->empty())
        continue;

      m_uses[fn] = uses;
      m_defs[fn] = defs;
    }
  }

  LOG(
      "dsa-cg", errs() << "--- USES ---\n"; for (auto kv
                                                 : m_uses) {
        errs() << kv.first->getName() << " ---> \n";
        for (auto CS : *(kv.second)) {
          errs() << "\t" << CS.getCaller()->getName() << ":";
	  CS.write(errs());
	  errs() << "\n";
        }
      } errs() << "--- DEFS ---\n";
      for (auto kv
           : m_defs) {
        errs() << kv.first->getName() << " ---> \n";
        for (auto CS : *(kv.second)) {
          errs() << "\t";
	  CS.write(errs());
	  errs() << "\n";
        }
      });
}
} // namespace sea_dsa
