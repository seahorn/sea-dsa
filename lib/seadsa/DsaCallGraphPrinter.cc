/*****
** Print the sea-dsa call graph to dot format.
*****/

#include "llvm/Pass.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/CallPrinter.h"
#include "llvm/Analysis/DOTGraphTraitsPass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/GraphWriter.h" 

#include "seadsa/config.h"
#include "seadsa/CompleteCallGraph.hh"
#include "seadsa/support/Debug.h"

using namespace llvm;

namespace seadsa {
extern std::string DotOutputDir;
}

namespace llvm {
template <>
struct DOTGraphTraits<CallGraph *> : public DefaultDOTGraphTraits {
  DOTGraphTraits(bool isSimple = false) : DefaultDOTGraphTraits(isSimple) {}

  static std::string getGraphName(CallGraph *CG) {
    return "Call graph: " + std::string(CG->getModule().getModuleIdentifier());
  }

  static bool isNodeHidden(const CallGraphNode *Node,
                         const CallGraph *CG) {
    if (Node->getFunction())
      return false;
    return true;
  }

  std::string getNodeLabel(const CallGraphNode *Node,
                           CallGraph *CG) {
    if (Node == CG->getExternalCallingNode())
      return "external caller";
    if (Node == CG->getCallsExternalNode())
      return "external callee";

    if (Function *Func = Node->getFunction())
      return std::string(Func->getName());
    return "external node";
  }
};
}

namespace seadsa {

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
  
  auto writeGraph = [&](std::string Filename, CallGraph &CG) {
    errs() << "Writing '" << Filename << "'...";

    std::error_code EC;
    raw_fd_ostream File(Filename, EC, sys::fs::OF_Text);

    std::string Title =
        DOTGraphTraits<CallGraph *>::getGraphName(&dsaCallGraph);
    const bool IsSimple = false;    
    if (!EC)
      WriteGraph(File, &CG, IsSimple, Title);
    else
      errs() << "  error opening file for writing!";
    errs() << "\n";
  };
  
  LOG("dsa-callgraph-pass",
      errs() << "*** SEA-DSA CALL GRAPH ***\n";
      dsaCallGraph.print(errs()););

  writeGraph(appendOutDir(m_outDir,"callgraph.dot"), dsaCallGraph);
  
  return false;
}

void DsaCallGraphPrinter::getAnalysisUsage(llvm::AnalysisUsage &AU) const {
  AU.setPreservesAll();
  AU.addRequired<CompleteCallGraph>();
}

Pass *createDsaCallGraphPrinterPass() {
  return new seadsa::DsaCallGraphPrinter();
}
} // namespace seadsa


static llvm::RegisterPass<seadsa::DsaCallGraphPrinter>
X("seadsa-callgraph-printer", "Print the SeaDsa call graph to dot format");


  
