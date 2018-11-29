#pragma once

#include "llvm/ADT/DenseMap.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"

#include "sea_dsa/CallSite.hh"
#include "sea_dsa/Graph.hh"

namespace llvm {
class DataLayout;
class TargetLibraryInfo;
class CallGraph;
} // namespace llvm

namespace sea_dsa {

class AllocWrapInfo;

class TopDownAnalysis {

public:
  typedef std::shared_ptr<Graph> GraphRef;
  typedef llvm::DenseMap<const llvm::Function *, GraphRef> GraphMap;

private:
  const llvm::DataLayout &m_dl;
  const llvm::TargetLibraryInfo &m_tli;
  const AllocWrapInfo &m_allocInfo;
  llvm::CallGraph &m_cg;
  bool m_noescape;

public:
  static void cloneAndResolveArguments(const DsaCallSite &CS, Graph &callerG,
                                       Graph &calleeG, bool noescape = true);

  TopDownAnalysis(const llvm::DataLayout &dl,
                  const llvm::TargetLibraryInfo &tli,
                  const AllocWrapInfo &allocInfo, llvm::CallGraph &cg,
                  bool noescape = true /* TODO: CLI*/)
      : m_dl(dl), m_tli(tli), m_allocInfo(allocInfo), m_cg(cg),
        m_noescape(noescape) {}

  bool runOnModule(llvm::Module &M, GraphMap &graphs);
};

} // namespace sea_dsa
