
#include "llvm/Pass.h"
#include "llvm/PassRegistry.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Registry.h"

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
llvm::Pass *createSpecGraphInfoPass() { return new SpecGraphInfo(); }

char SpecGraphInfo::ID = 0;

bool SpecGraphInfo::runOnModule(llvm::Module &m) { return false; }

} // namespace seadsa

using namespace seadsa;
using namespace llvm;

INITIALIZE_PASS_BEGIN(SpecGraphInfo, "seadsa-spec-graph-info",
                      "Creates local analysis from spec", false, false)
INITIALIZE_PASS_END(SpecGraphInfo, "seadsa-spec-graph-info",
                    "Creates local analysis from spec", false, false)
