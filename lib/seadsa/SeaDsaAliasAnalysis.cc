#include "seadsa/SeaDsaAliasAnalysis.hh"

#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/CFLAliasAnalysisUtils.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Type.h"
#include "seadsa/AllocWrapInfo.hh"
#include "seadsa/InitializePasses.hh"
#include "seadsa/support/Debug.h"
#include "seadsa/Global.hh"
#include "seadsa/Graph.hh"


#define DEBUG_TYPE "sea-aa"
using namespace llvm;
using namespace seadsa;
namespace dsa = seadsa;

namespace seadsa {
  
SeaDsaAAResult::SeaDsaAAResult(
    TargetLibraryInfoWrapperPass &tliWrapper,
    AllocWrapInfo &awi)
  : m_tliWrapper(tliWrapper), m_awi(awi), m_fac(nullptr), m_dsa(nullptr)  {}
  
SeaDsaAAResult::SeaDsaAAResult(SeaDsaAAResult &&RHS)
  : AAResultBase(std::move(RHS)), m_tliWrapper(RHS.m_tliWrapper), m_awi(RHS.m_awi),
    m_fac(std::move(RHS.m_fac)), m_dsa(std::move(RHS.m_dsa)) {}
  
SeaDsaAAResult::~SeaDsaAAResult() = default;

static Module* getModuleFromQuery(Value *ValA, Value *ValB) {
  Function *MaybeFnA = const_cast<Function *>(llvm::cflaa::parentFunctionOfValue(ValA));
  Function *MaybeFnB = const_cast<Function *>(llvm::cflaa::parentFunctionOfValue(ValB));
  if (!MaybeFnA && !MaybeFnB) {
    // The only times this is known to happen are when globals + InlineAsm are
    // involved
    DOG(llvm::errs()
	<< "SeaDsaAA: could not extract parent function information.\n");
    return nullptr;
  }
  Module *M = nullptr;
  if (MaybeFnA) {
    M = MaybeFnA->getParent();
  }
  if (MaybeFnB) {
    if (M != MaybeFnA->getParent()) {
      DOG(llvm::errs()
	  << "SeaDsaAA: cannot handle functions in different modules.\n");
      return nullptr;
    }
  }
  return M;
}

static uint64_t storageSize(const Type *t, const DataLayout &dl) {
  return dl.getTypeStoreSize(const_cast<Type *>(t));
}

static Optional<uint64_t> sizeOf(const Graph::Set &types, const DataLayout &dl) {
  if (types.isEmpty()) {
    return 0;
  } else {
    uint64_t sz = storageSize(*(types.begin()), dl);
    if (types.isSingleton()) {
      return sz;
    } else {
      auto it = types.begin();
      ++it;
      if (std::all_of(it, types.end(), [dl, sz](const Type *t) {
	      return (storageSize(t, dl) == sz);
          })) {
        return sz;
      } else {
        return None;
      }
    }
  }
}
  
static bool mayAlias(const Cell &c1, const Cell &c2, const DataLayout &dl) {
  auto maybeUnsafe = [](const Node *n) {
		       return n->isIntToPtr() || n->isPtrToInt() ||
			 n->isIncomplete() || n->isUnknown();
		     };
  
  if (c1.isNull() || c2.isNull()) {
    return true;
  }
  
  const Node *n1 = c1.getNode();
  const Node *n2 = c2.getNode();

  if (maybeUnsafe(n1) || maybeUnsafe(n2)) {
    return true;
  }

  if (n1 != n2) {
    // different nodes cannot alias
    return false;
  }

  if (n1->isOffsetCollapsed() || n1->isArray()) {
    return true;
  }

  unsigned o1, o2;
  
  if (c1.getOffset() <= c2.getOffset()) {
    o1 = c1.getOffset();
    o2 = c2.getOffset();
  } else {
    o1 = c2.getOffset();
    o2 = c1.getOffset();
  } 

  assert(o1 <= o2);
  
  if (!n1->hasAccessedType(o1)) {
    return true;
  }

  auto sizeOfOffset1 = sizeOf(n1->getAccessedType(o1), dl);
  if (!sizeOfOffset1.hasValue()) {
    return true;
  }
  
  // if offsets can overlap then may alias
  return (o1 + sizeOfOffset1.getValue()) >= o2;
}

			
llvm::AliasResult SeaDsaAAResult::alias(const llvm::MemoryLocation &LocA,
                                        const llvm::MemoryLocation &LocB, llvm::AAQueryInfo &AAQI) {
  DOG(llvm::errs() << "SeaDsaAA --- Alias query: " << *LocA.Ptr << " and " << *LocB.Ptr << "\n\n";);

  auto *ValA = const_cast<Value *>(LocA.Ptr);
  auto *ValB = const_cast<Value *>(LocB.Ptr);

  if (!ValA->getType()->isPointerTy() || !ValB->getType()->isPointerTy()) {
    return NoAlias;
  }
  
  if (ValA == ValB) {
    return MustAlias;
  }

  const DataLayout *dl = nullptr;
  // Run seadsa if we have not done it yet
  if (!m_dsa) {
    if (Module *M = getModuleFromQuery(ValA, ValB)) {
      m_fac = std::make_unique<Graph::SetFactory>();
      dl = &(M->getDataLayout());
      CallGraph cg(*M); 
      m_dsa = std::make_unique<BottomUpTopDownGlobalAnalysis>(*dl, m_tliWrapper, m_awi, cg, *m_fac);
      DOG(llvm::errs() << "Running SeaDsaAA.\n");
      m_dsa->runOnModule(*M);
    } 
  }

  // We tried to run seadsa but we couldn't 
  if (!m_dsa) {
    return AAResultBase::alias(LocA, LocB, AAQI);
  }

  Function *FnA = const_cast<Function *>(llvm::cflaa::parentFunctionOfValue(ValA));
  Function *FnB = const_cast<Function *>(llvm::cflaa::parentFunctionOfValue(ValB));
  if (!FnA || !FnB) {
    return AAResultBase::alias(LocA, LocB, AAQI);
  }

  assert(m_dsa);
  assert(dl);
  
  Graph &gA = m_dsa->getGraph(*FnA);
  Graph &gB = m_dsa->getGraph(*FnB);  

  if (&gA != &gB) {
    DOG(llvm::errs() << "SeaDsaAA does not handle inter-procedural queries at the moment.\n");
    return AAResultBase::alias(LocA, LocB, AAQI);
  }

  if (gA.hasCell(*ValA) && gA.hasCell(*ValB)) {
    const Cell &c1 = gA.getCell(*ValA);
    const Cell &c2 = gA.getCell(*ValB);
    if (!mayAlias(c1,c2,*dl)) {
      return NoAlias;
    }
  }
  
  // -- fall back to default implementation    
  return AAResultBase::alias(LocA, LocB, AAQI);
}

// AnalysisKey SeaDsaAA::Key;

// SeaDsaAAResult SeaDsaAA::run(Function &F, FunctionAnalysisManager &AM) {
//   auto GetTLI = [&AM](Function &F) -> TargetLibraryInfo & {
//     return AM.getResult<TargetLibraryAnalysis>(F);
//   };
//   // FIXME: how to get AllocWrapInfo from AM ?
//   return SeaDsaAAResult(GetTLI);
// }

char SeaDsaAAWrapperPass::ID = 0;

ImmutablePass *seadsa::createSeaDsaAAWrapperPass() {
  return new SeaDsaAAWrapperPass();
}

SeaDsaAAWrapperPass::SeaDsaAAWrapperPass() : ImmutablePass(ID) {
  initializeSeaDsaAAWrapperPassPass(*PassRegistry::getPassRegistry());
}

void SeaDsaAAWrapperPass::initializePass() {
  DOG(errs() << "initializing SeaDsaAAWrapperPass\n");
  auto &tliWrapper = this->getAnalysis<TargetLibraryInfoWrapperPass>();
  auto &awi = this->getAnalysis<AllocWrapInfo>();
  Result.reset(new SeaDsaAAResult(tliWrapper, awi));
}

void SeaDsaAAWrapperPass::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesAll();
  AU.addRequired<TargetLibraryInfoWrapperPass>();
  AU.addRequired<AllocWrapInfo>();
}
}

namespace llvm {
INITIALIZE_PASS(SeaDsaAAWrapperPass, "seadsa-aa",
                "SeaDsa-Based Alias Analysis", false, true)
}
