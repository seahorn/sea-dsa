#pragma once

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"

#include "sea_dsa/CallSite.hh"
#include "sea_dsa/Graph.hh"
#include "sea_dsa/Mapper.hh"

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
  typedef std::shared_ptr<SimulationMapper> SimulationMapperRef;

  const llvm::DataLayout &m_dl;
  const llvm::TargetLibraryInfo &m_tli;
  const AllocWrapInfo &m_allocInfo;
  llvm::CallGraph &m_cg;
  bool m_flowSensitiveOpt;

public:

  static void cloneAndResolveArguments(const DsaCallSite &CS, Graph &calleeG,
                                       Graph &callerG, bool flowSensitiveOpt = true);

  BottomUpAnalysis(const llvm::DataLayout &dl,
                   const llvm::TargetLibraryInfo &tli,
                   const AllocWrapInfo &allocInfo, llvm::CallGraph &cg,
		   bool flowSensitiveOpt = true)
    : m_dl(dl), m_tli(tli), m_allocInfo(allocInfo), m_cg(cg),
      m_flowSensitiveOpt(flowSensitiveOpt) {}

  bool runOnModule(llvm::Module &M, GraphMap &graphs);
};

} // namespace sea_dsa
