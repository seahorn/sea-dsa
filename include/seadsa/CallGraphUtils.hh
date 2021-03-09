#pragma once

#include "seadsa/CallSite.hh"
#include "seadsa/DsaLibFuncInfo.hh"
#include "llvm/ADT/Optional.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Analysis/CallGraph.h"

namespace seadsa {
namespace call_graph_utils {

// Return a dsa callsite from a llvm CallGraph edge
template <typename CallRecord>
llvm::Optional<DsaCallSite>
getDsaCallSite(CallRecord &callRecord, const DsaLibFuncInfo *dlfi = nullptr) {
  const llvm::Function *callee = callRecord.second->getFunction();
  if (!callee) return llvm::None;
  if (callee->isDeclaration() || callee->empty()) {
    if (!dlfi || !dlfi->hasSpecFunc(*callee)) return llvm::None;
  }

  if (dlfi && dlfi->hasSpecFunc(*callee)) callee = dlfi->getSpecFunc(*callee);

  auto &cs = *llvm::dyn_cast<llvm::CallBase>(*callRecord.first);
  if (cs.isIndirectCall()) {
    DsaCallSite dsaCS(cs, *callee);
    return (dsaCS.getCallee() ? llvm::Optional<DsaCallSite>(dsaCS)
                              : llvm::None);
  } else {
    DsaCallSite dsaCS(cs);
    return (dsaCS.getCallee() ? llvm::Optional<DsaCallSite>(dsaCS)
                              : llvm::None);
  }
}

template <typename T>
std::vector<llvm::CallGraphNode *> SortedCGNs(const T &t) {
  std::vector<llvm::CallGraphNode *> cgns;
  for (llvm::CallGraphNode *cgn : t) {
    llvm::Function *fn = cgn->getFunction();
    if (!fn || fn->empty()) continue;

    cgns.push_back(cgn);
  }

  std::stable_sort(cgns.begin(), cgns.end(),
                   [](llvm::CallGraphNode *first, llvm::CallGraphNode *snd) {
                     return first->getFunction()->getName() <
                            snd->getFunction()->getName();
                   });

  return cgns;
}

inline std::vector<const llvm::Value *>
SortedCallSites(llvm::CallGraphNode *cgn,
                const DsaLibFuncInfo &dsaLibFuncInfo) {
  std::vector<const llvm::Value *> res;
  res.reserve(cgn->size());

  for (auto &callRecord : *cgn) {
    DsaCallSite dsaCS(*llvm::dyn_cast<llvm::CallBase>(*callRecord.first));

    const llvm::Function *callee_func = dsaCS.getCallee();
    if (!callee_func) continue;

    const llvm::Function &callee =
        dsaLibFuncInfo.hasSpecFunc(*callee_func)
            ? *dsaLibFuncInfo.getSpecFunc(*callee_func)
            : *callee_func;

    if (callee.isDeclaration() || callee.empty()) continue;
    res.push_back(*callRecord.first);
  }

  std::stable_sort(res.begin(), res.end(),
                   [](const llvm::Value *first, const llvm::Value *snd) {
                     DsaCallSite dsaCS1(*llvm::dyn_cast<llvm::CallBase>(first));
                     llvm::StringRef callerN1 = dsaCS1.getCaller()->getName();
                     llvm::StringRef calleeN1 = dsaCS1.getCallee()->getName();

                     DsaCallSite dsaCS2(*llvm::dyn_cast<llvm::CallBase>(snd));
                     llvm::StringRef callerN2 = dsaCS2.getCaller()->getName();
                     llvm::StringRef calleeN2 = dsaCS2.getCallee()->getName();

                     return std::make_pair(callerN1, calleeN1) <
                            std::make_pair(callerN2, calleeN2);
                   });

  return res;
}

} // namespace call_graph_utils
} // namespace seadsa
