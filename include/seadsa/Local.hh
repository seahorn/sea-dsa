#pragma once

#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"

#include "seadsa/Graph.hh"

namespace llvm {
class DataLayout;
class TargetLibraryInfoWrapperPass;
class TargetLibraryInfo;
} // namespace llvm

namespace seadsa {
class AllocWrapInfo;

class LocalAnalysis {
  const llvm::DataLayout &m_dl;
  llvm::TargetLibraryInfoWrapperPass &m_tliWrapper;
  const seadsa::AllocWrapInfo &m_allocInfo;
  bool m_track_callsites;

public:
  LocalAnalysis(const llvm::DataLayout &dl,
                llvm::TargetLibraryInfoWrapperPass &tliWrapper,
                const AllocWrapInfo &allocInfo, bool track_callsites = false)
      : m_dl(dl), m_tliWrapper(tliWrapper), m_allocInfo(allocInfo),
        m_track_callsites(track_callsites) {}

  void runOnFunction(llvm::Function &F, Graph &g);
};

class Local : public llvm::ModulePass {
  Graph::SetFactory m_setFactory;
  typedef std::shared_ptr<Graph> GraphRef;
  llvm::DenseMap<const llvm::Function *, GraphRef> m_graphs;

  const llvm::DataLayout *m_dl;
  const AllocWrapInfo *m_allocInfo;

public:
  static char ID;

  Local();
  void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;
  bool runOnModule(llvm::Module &M) override;
  bool runOnFunction(llvm::Function &F);
  llvm::StringRef getPassName() const override { return "SeaDsa local pass"; }
  bool hasGraph(const llvm::Function &F) const;
  const Graph &getGraph(const llvm::Function &F) const;
};
} // namespace seadsa
