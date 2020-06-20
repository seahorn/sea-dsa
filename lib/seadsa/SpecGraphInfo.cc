
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
std::string SpecFile;
}

static llvm::cl::opt<std::string, true>
    XSpecFile("sea-dsa-specfile", llvm::cl::desc("<Input spec bitcode file>"),
              llvm::cl::Optional, llvm::cl::location(seadsa::SpecFile),
              llvm::cl::init(""), llvm::cl::value_desc("filename"));

namespace seadsa {

void SpecGraphInfo::getAnalysisUsage(llvm::AnalysisUsage &AU) const {
  AU.addRequired<seadsa::AllocWrapInfo>();
  AU.addRequired<llvm::TargetLibraryInfoWrapperPass>();
  AU.setPreservesAll();
}

void SpecGraphInfo::initialize() const {

  using namespace llvm::sys::fs;
  using namespace llvm;

  m_awi = &getAnalysis<AllocWrapInfo>();
  m_tliWrapper = &getAnalysis<TargetLibraryInfoWrapperPass>();

  if (SpecFile.empty()) return;

  SMDiagnostic err;

  m_module = parseIRFile(SpecFile, err, m_ctx);

  auto &dl = m_module->getDataLayout();

  LocalAnalysis la(dl, *m_tliWrapper, *m_awi);

  auto &spec_funcs = m_module->getFunctionList();

  for (llvm::Function &func : spec_funcs) {
    if (func.isDeclaration() || func.empty()) continue;
    m_funcs.insert({func.getName().str(), func});
  }
} // namespace seadsa

bool SpecGraphInfo::hasSpecFunc(const llvm::Function &F) const {
  return m_funcs.count(F.getName().str());
}

llvm::Function &SpecGraphInfo::getSpecFunc(const llvm::Function &F) const {
  auto it = m_funcs.find(F.getName().str());
  assert(it != m_funcs.end());
  return it->second;
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
