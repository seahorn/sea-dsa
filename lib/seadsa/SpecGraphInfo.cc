
#include "llvm/ADT/Twine.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Pass.h"
#include "llvm/PassRegistry.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/Registry.h"

#include "seadsa/Graph.hh"
#include "seadsa/InitializePasses.hh"
#include "seadsa/SpecGraphInfo.hh"
#include "seadsa/support/Debug.h"
#include <iostream>

namespace seadsa {
std::string SpecDir;
}

static llvm::cl::opt<std::string, true>
    XSpecDir("sea-dsa-specdir", llvm::cl::desc("<Input spec file directory>"),
             llvm::cl::Optional, llvm::cl::location(seadsa::SpecDir),
             llvm::cl::init(""), llvm::cl::value_desc("DIR"));

namespace seadsa {

void SpecGraphInfo::getAnalysisUsage(llvm::AnalysisUsage &AU) const {
  AU.addRequired<seadsa::AllocWrapInfo>();
  AU.addRequired<llvm::TargetLibraryInfoWrapperPass>();
  AU.setPreservesAll();
}

bool SpecGraphInfo::runOnModule(llvm::Module &m) {
  m_awi = &getAnalysis<seadsa::AllocWrapInfo>();
  m_tliWrapper = &getAnalysis<llvm::TargetLibraryInfoWrapperPass>();

  getAvailableSpecs();
  return false;
}

void SpecGraphInfo::getAvailableSpecs() {
  using namespace llvm::sys::fs;
  using namespace llvm;

  Twine t(SpecDir);
  std::error_code ec = std::error_code();

  if (SpecDir.empty()) return;

  StringRef ref = SpecDir;

  for (auto it = directory_iterator(t, ec); it != directory_iterator();
       it.increment(ec)) {
    auto a = it->path();
    if (!(it->type() == file_type::regular_file)) continue;

    auto path = StringRef(it->path());
    auto extension = llvm::sys::path::extension(path);
    if (!extension.equals(".ll") && !extension.equals(".bc")) continue;

    SMDiagnostic err;
    LLVMContext context;

    auto module = parseIRFile(path, err, context);
    auto &dl = module->getDataLayout();

    LocalAnalysis la(dl, *m_tliWrapper, *m_awi);

    auto &spec_funcs = module->getFunctionList();

    for (Function &func : spec_funcs) {
      if (func.isDeclaration() || func.empty()) continue;
      GraphRef graph = std::make_shared<Graph>(dl, m_setFactory);
      la.runOnFunction(func, *graph);
      m_graphs.insert({&func, graph});
    }
  }
}

bool SpecGraphInfo::hasGraph(const llvm::Function &F) const {
  return m_graphs.count(&F);
}

Graph &SpecGraphInfo::getGraph(const llvm::Function &F) const {
  auto it = m_graphs.find(&F);
  assert(it != m_graphs.end());
  return *(it->second);
}

char SpecGraphInfo::ID = 0;
} // namespace seadsa

namespace seadsa {
llvm::Pass *createSpecGraphInfoPass() { return new SpecGraphInfo(); }
} // namespace seadsa

using namespace seadsa;
using namespace llvm;

INITIALIZE_PASS_BEGIN(SpecGraphInfo, "seadsa-spec-graph-info",
                      "Creates local analysis from spec", false, false)
INITIALIZE_PASS_DEPENDENCY(AllocWrapInfo)
INITIALIZE_PASS_DEPENDENCY(TargetLibraryInfoWrapperPass)
INITIALIZE_PASS_END(SpecGraphInfo, "seadsa-spec-graph-info",
                    "Creates local analysis from spec", false, false)
