#ifndef __DSA_BOTTOM_UP_HH_
#define __DSA_BOTTOM_UP_HH_

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/StringRef.h"
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

class BottomUpAnalysis {

public:
  typedef std::shared_ptr<Graph> GraphRef;
  typedef llvm::DenseMap<const llvm::Function *, GraphRef> GraphMap;

private:
  const llvm::DataLayout &m_dl;
  const llvm::TargetLibraryInfo &m_tli;
  const AllocWrapInfo &m_allocInfo;
  llvm::CallGraph &m_cg;
  // true if assume that alloca allocated (stack) memory does not escape
  bool m_noescape;

public:
  static void cloneAndResolveArguments(const DsaCallSite &CS, Graph &calleeG,
                                       Graph &callerG, bool noescape = true);

  BottomUpAnalysis(const llvm::DataLayout &dl,
                   const llvm::TargetLibraryInfo &tli,
                   const AllocWrapInfo &allocInfo, llvm::CallGraph &cg,
                   bool noescape = true /* TODO: CLI*/)
      : m_dl(dl), m_tli(tli), m_allocInfo(allocInfo), m_cg(cg),
        m_noescape(noescape) {}

  bool runOnModule(llvm::Module &M, GraphMap &graphs);
};

class BottomUp : public llvm::ModulePass {
  typedef typename BottomUpAnalysis::GraphRef GraphRef;
  typedef typename BottomUpAnalysis::GraphMap GraphMap;

  Graph::SetFactory m_setFactory;
  const llvm::DataLayout *m_dl;
  const llvm::TargetLibraryInfo *m_tli;
  const AllocWrapInfo *m_allocInfo;
  GraphMap m_graphs;

public:
  static char ID;

  BottomUp();

  void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;

  bool runOnModule(llvm::Module &M) override;

  llvm::StringRef getPassName() const override { return "BottomUp DSA pass"; }

  Graph &getGraph(const llvm::Function &F) const;
  bool hasGraph(const llvm::Function &F) const;
};
} // namespace sea_dsa
#endif
