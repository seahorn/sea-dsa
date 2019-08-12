#pragma once

#include "llvm/IR/CallSite.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/ADT/Optional.h"
#include "llvm/ADT/StringRef.h"
#include "sea_dsa/CallSite.hh"

namespace sea_dsa{
namespace call_graph_utils {

// Return a dsa callsite from a llvm CallGraph edge
template<typename CallRecord>
llvm::Optional<DsaCallSite> getDsaCallSite(CallRecord& callRecord) {
  const llvm::Function* callee = callRecord.second->getFunction();
  if (!callee || callee->isDeclaration() || callee->empty())
    return llvm::None;
  
  llvm::CallSite CS(callRecord.first);
  if (CS.isIndirectCall()) {
    DsaCallSite dsaCS(*CS.getInstruction(), *callee);
    return (dsaCS.getCallee() ? llvm::Optional<DsaCallSite>(dsaCS) : llvm::None);    
  } else {
    DsaCallSite dsaCS(*CS.getInstruction());
    return (dsaCS.getCallee() ? llvm::Optional<DsaCallSite>(dsaCS) : llvm::None);
  }
}

template <typename T>
std::vector<llvm::CallGraphNode *> SortedCGNs(const T &t) {
  std::vector<llvm::CallGraphNode *> cgns;
  for (llvm::CallGraphNode *cgn : t) {
    llvm::Function *fn = cgn->getFunction();
    if (!fn || fn->isDeclaration() || fn->empty())
      continue;

    cgns.push_back(cgn);
  }

  std::stable_sort(
      cgns.begin(), cgns.end(), [](llvm::CallGraphNode *first, llvm::CallGraphNode *snd) {
        return first->getFunction()->getName() < snd->getFunction()->getName();
      });

  return cgns;
}

inline std::vector<const llvm::Value *> SortedCallSites(llvm::CallGraphNode *cgn) {
  std::vector<const llvm::Value *> res;
  res.reserve(cgn->size());

  for (auto &callRecord : *cgn) {
    llvm::ImmutableCallSite CS(callRecord.first);
    DsaCallSite dsaCS(CS);
    const llvm::Function *callee = dsaCS.getCallee();
    if (!callee || callee->isDeclaration() || callee->empty())
      continue;

    res.push_back(callRecord.first);
  }

  std::stable_sort(res.begin(), res.end(),
                   [](const llvm::Value *first, const llvm::Value *snd) {
		     llvm::ImmutableCallSite CS1(first);
                     DsaCallSite dsaCS1(CS1);
                     llvm::StringRef callerN1 = dsaCS1.getCaller()->getName();
                     llvm::StringRef calleeN1 = dsaCS1.getCallee()->getName();

		     llvm::ImmutableCallSite CS2(snd);
                     DsaCallSite dsaCS2(CS2);
                     llvm::StringRef callerN2 = dsaCS2.getCaller()->getName();
                     llvm::StringRef calleeN2 = dsaCS2.getCallee()->getName();

                     return std::make_pair(callerN1, calleeN1) <
                            std::make_pair(callerN2, calleeN2);
                   });

  return res;
}


} // end namespace
} // end namespace
