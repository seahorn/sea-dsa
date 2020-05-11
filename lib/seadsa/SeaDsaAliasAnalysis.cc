#include "seadsa/SeaDsaAliasAnalysis.hh"

#include "llvm/IR/Function.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Type.h"

#include "seadsa/InitializePasses.hh"

using namespace llvm;
using namespace seadsa;
namespace dsa = seadsa;

namespace seadsa {
SeaDsaAAResult::SeaDsaAAResult(
    std::function<const TargetLibraryInfo &(Function &F)> GetTLI)
    : GetTLI(std::move(GetTLI)) {}
SeaDsaAAResult::SeaDsaAAResult(SeaDsaAAResult &&RHS)
    : AAResultBase(std::move(RHS)), GetTLI(std::move(RHS.GetTLI)) {}
SeaDsaAAResult::~SeaDsaAAResult() = default;

llvm::AliasResult SeaDsaAAResult::alias(const llvm::MemoryLocation &LocA,
                                        const llvm::MemoryLocation &LocB, llvm::AAQueryInfo &AAQI) {
  if (LocA.Ptr == LocB.Ptr) return MustAlias;
  // -- fall back to default implementation
  return AAResultBase::alias(LocA, LocB, AAQI);
}

AnalysisKey SeaDsaAA::Key;

SeaDsaAAResult SeaDsaAA::run(Function &F, FunctionAnalysisManager &AM) {
  auto GetTLI = [&AM](Function &F) -> TargetLibraryInfo & {
    return AM.getResult<TargetLibraryAnalysis>(F);
  };
  return SeaDsaAAResult(GetTLI);
}

char SeaDsaAAWrapperPass::ID = 0;

ImmutablePass *seadsa::createSeaDsaAAWrapperPass() {
  return new SeaDsaAAWrapperPass();
}

SeaDsaAAWrapperPass::SeaDsaAAWrapperPass() : ImmutablePass(ID) {
  initializeSeaDsaAAWrapperPassPass(*PassRegistry::getPassRegistry());
}

void SeaDsaAAWrapperPass::initializePass() {
  auto GetTLI = [this](Function &F) -> TargetLibraryInfo & {
    return this->getAnalysis<TargetLibraryInfoWrapperPass>().getTLI(F);
  };
  Result.reset(new SeaDsaAAResult(GetTLI));
}

void SeaDsaAAWrapperPass::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesAll();
  AU.addRequired<TargetLibraryInfoWrapperPass>();
}
}

namespace llvm {
INITIALIZE_PASS(SeaDsaAAWrapperPass, "seadsa-aa",
                "SeaDsa-Based Alias Analysis", false, true)
}
