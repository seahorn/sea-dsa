#pragma once

#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Module.h"

#include "seadsa/CompleteCallGraph.hh"
#include "seadsa/Global.hh"

#include <memory>

namespace seadsa {

class DsaPrinterImpl;

/**
 * Print seadsa graphs in dot format.
 *
 * If the analysis is context-sensitive then the _global_ seadsa graph
 * of each function FUN is printed in the file FUN.mem.dot. If the
 * analysis is context-insensitive then only one _global_ seadsa graph
 * is printed for the main function. By global, we mean the results of
 * the corresponding analysis upon completion. In addition,
 * summaries graphs are printed if printSummaryGraphs is enabled.
 **/
class DsaPrinter {
  std::unique_ptr<DsaPrinterImpl> m_impl;

public:
  DsaPrinter(GlobalAnalysis &dsa, CompleteCallGraph *ccg,
             bool printSummaryGraphs = false);

  ~DsaPrinter();

  bool runOnModule(llvm::Module &M);
};

class DsaPrinterPass : public llvm::ModulePass {
public:
  static char ID;

  DsaPrinterPass();

  bool runOnModule(llvm::Module &M) override;

  void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;

  llvm::StringRef getPassName() const override;
};

} // end namespace seadsa
