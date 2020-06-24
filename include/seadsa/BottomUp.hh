#pragma once

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"

#include "seadsa/CallSite.hh"
#include "seadsa/Graph.hh"
#include "seadsa/Mapper.hh"

namespace llvm {
class DataLayout;
class TargetLibraryInfo;
class TargetLibraryInfoWrapperPass;
class CallGraph;
} // namespace llvm

namespace seadsa {
class AllocWrapInfo;
class DsaLibFuncInfo;

class BottomUpAnalysis {

public:
  typedef std::shared_ptr<Graph> GraphRef;
  typedef llvm::DenseMap<const llvm::Function *, GraphRef> GraphMap;

private:
  typedef std::shared_ptr<SimulationMapper> SimulationMapperRef;

  const llvm::DataLayout &m_dl;
  llvm::TargetLibraryInfoWrapperPass &m_tliWrapper;
  const AllocWrapInfo &m_allocInfo;
  const DsaLibFuncInfo &m_dsaLibFuncInfo;
  llvm::CallGraph &m_cg;
  bool m_flowSensitiveOpt;

public:
  static void cloneAndResolveArguments(const DsaCallSite &CS, Graph &calleeG,
                                       Graph &callerG,
                                       const DsaLibFuncInfo &dsaLibFuncInfo,
                                       bool flowSensitiveOpt = true);

  BottomUpAnalysis(const llvm::DataLayout &dl,
                   llvm::TargetLibraryInfoWrapperPass &tliWrapper,
                   const AllocWrapInfo &allocInfo,
                   const DsaLibFuncInfo &dsaLibFuncInfo, llvm::CallGraph &cg,
                   bool flowSensitiveOpt = true)
      : m_dl(dl), m_tliWrapper(tliWrapper), m_allocInfo(allocInfo),
        m_dsaLibFuncInfo(dsaLibFuncInfo), m_cg(cg),
        m_flowSensitiveOpt(flowSensitiveOpt) {}

  bool runOnModule(llvm::Module &M, GraphMap &graphs);
};

} // namespace seadsa
