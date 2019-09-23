#pragma once

#include "llvm/ADT/DenseMap.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"

#include "sea_dsa/CallSite.hh"
#include "sea_dsa/Graph.hh"

namespace llvm {
class CallGraph;
} // namespace llvm

namespace sea_dsa {

class TopDownAnalysis {

public:
  typedef std::shared_ptr<Graph> GraphRef;
  typedef llvm::DenseMap<const llvm::Function *, GraphRef> GraphMap;

private:
  llvm::CallGraph &m_cg;
  bool m_flowSensitiveOpt;
  bool m_noescape;

public:
  static void cloneAndResolveArguments(const DsaCallSite &CS, Graph &callerG,
                                       Graph &calleeG, bool flowSensitiveOpt = true, 
				       bool noescape = true);

  TopDownAnalysis(llvm::CallGraph &cg,
		  bool flowSensitiveOpt = true,
                  bool noescape = true /* TODO: CLI*/)
      : m_cg(cg),
        m_flowSensitiveOpt(flowSensitiveOpt), m_noescape(noescape) {}

  bool runOnModule(llvm::Module &M, GraphMap &graphs);
};

} // namespace sea_dsa
