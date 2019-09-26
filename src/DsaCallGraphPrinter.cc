/*****
** Print the sea-dsa call graph to dot format.
*****/

#include "llvm/Pass.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/CFGPrinter.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/FileSystem.h"

#include "sea_dsa/config.h"
#include "sea_dsa/CompleteCallGraph.hh"
#include "sea_dsa/support/Debug.h"

using namespace llvm;

namespace sea_dsa {
extern std::string DotOutputDir;
}


namespace sea_dsa {

struct DsaCallGraphPrinter: public llvm::ModulePass {
  static char ID;
  std::string m_outDir;
  
  DsaCallGraphPrinter()
    : llvm::ModulePass(ID), m_outDir(DotOutputDir) {}

  bool runOnModule(llvm::Module &M) override;
  
  void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;

  virtual llvm::StringRef getPassName() const override {
    return "Print SeaDsa call graph to dot format";
  }
};

char DsaCallGraphPrinter::ID = 0;

static std::string appendOutDir(std::string OutputDir, std::string FileName) {
  if (!OutputDir.empty()) {
    if (!llvm::sys::fs::create_directory(OutputDir)) {
	std::string FullFileName = OutputDir + "/" + FileName;
	return FullFileName;
    }
  }
  return FileName;
}

bool DsaCallGraphPrinter::runOnModule(Module &M) {

  auto &CCG = getAnalysis<CompleteCallGraph>();
  auto &dsaCallGraph = CCG.getCompleteCallGraph();
  
  auto writeGraph = [&dsaCallGraph](std::string Filename) {
    std::error_code EC;
    errs() << "Writing '" << Filename << "'...";
    raw_fd_ostream File(Filename, EC, sys::fs::F_Text);
    std::string Title("Call graph");
    const bool IsSimple = false;
    if (!EC)
      WriteGraph(File, &dsaCallGraph, IsSimple, Title);
    else
      errs() << "  error opening file for writing!";
    errs() << "\n";
  };
  
  LOG("dsa-callgraph-pass",
      errs() << "*** SEA-DSA CALL GRAPH ***\n";
      dsaCallGraph.print(errs()););

  writeGraph(appendOutDir(m_outDir,"callgraph.dot"));
  
  return false;
}

void DsaCallGraphPrinter::getAnalysisUsage(llvm::AnalysisUsage &AU) const {
  AU.setPreservesAll();
  AU.addRequired<CompleteCallGraph>();
}

Pass *createDsaCallGraphPrinterPass() {
  return new sea_dsa::DsaCallGraphPrinter();
}
} // namespace sea_dsa


static llvm::RegisterPass<sea_dsa::DsaCallGraphPrinter>
X("seadsa-callgraph-printer", "Print the SeaDsa call graph to dot format");


  
