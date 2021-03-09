#include "seadsa/AllocWrapInfo.hh"
#include "seadsa/InitializePasses.hh"

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/MemoryBuiltins.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"
#include "llvm/Support/CommandLine.h"
#include <vector>

using namespace seadsa;
using namespace llvm;

static llvm::cl::list<std::string>
    ExtraAllocs("alloc-function", llvm::cl::desc("malloc-like functions"),
                llvm::cl::ZeroOrMore);

static llvm::cl::list<std::string>
    ExtraFrees("dealloc-function", llvm::cl::desc("free-like functions"),
               llvm::cl::ZeroOrMore);

namespace {
bool isNotStored(Value *V);
}

void AllocWrapInfo::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<TargetLibraryInfoWrapperPass>();
  AU.setPreservesAll();
}

void AllocWrapInfo::initialize(Module &M, Pass *P) const {
  if (!m_tliWrapper) {
    m_tliWrapper = getAnalysisIfAvailable<TargetLibraryInfoWrapperPass>();
  }
  
  if (!m_tliWrapper) {
    llvm::errs() << "ERROR: AllocWrapInfo::initialize needs TargetLibraryInfo\n";
    assert(false);
    return;
  }
  
  for (auto &name : ExtraAllocs) {
    m_allocs.insert(name);
  }
  for (auto &name : ExtraFrees) {
    m_allocs.insert(name);
  }
  // C
  m_allocs.insert("malloc");
  m_allocs.insert("calloc");
  // m_allocs.insert("realloc");
  // m_allocs.insert("memset");
  // C++
  m_allocs.insert("Znwj");               // new(unsigned int)
  m_allocs.insert("ZnwjRKSt9nothrow_t"); // new(unsigned int, nothrow)
  m_allocs.insert("Znwm");               // new(unsigned long)
  m_allocs.insert("ZnwmRKSt9nothrow_t"); // new(unsigned long, nothrow)
  m_allocs.insert("Znaj");               // new[](unsigned int)
  m_allocs.insert("ZnajRKSt9nothrow_t"); // new[](unsigned int, nothrow)
  m_allocs.insert("Znam");               // new[](unsigned long)
  m_allocs.insert("ZnamRKSt9nothrow_t"); // new[](unsigned long, nothrow)
  // C
  m_deallocs.insert("free");
  m_deallocs.insert("cfree");
  // C++
  m_deallocs.insert("ZdlPv");               // operator delete(void*)
  m_deallocs.insert("ZdaPv");               // operator delete[](void*)
  m_deallocs.insert("ZdlPvj");              // delete(void*, uint)
  m_deallocs.insert("ZdlPvm");              // delete(void*, ulong)
  m_deallocs.insert("ZdlPvRKSt9nothrow_t"); // delete(void*, nothrow)
  m_deallocs.insert("ZdaPvj");              // delete[](void*, uint)
  m_deallocs.insert("ZdaPvm");              // delete[](void*, ulong)
  m_deallocs.insert("ZdaPvRKSt9nothrow_t"); // delete[](void*, nothrow)

  // find allocation sites known to LTI
  findAllocs(M);
  while (findWrappers(M, P, m_allocs)) { /* skip */
    ;
  }
  while (findWrappers(M, P, m_deallocs)) { /* skip */
    ;
  }
}

void AllocWrapInfo::findAllocs(Module &M) const {
  for (Function &F : M) {
    if (m_allocs.count(F.getName()))
      continue;
    auto &tli = getTLI(F);
    // find the first call because MemoryBuiltins only provides
    // interface for call-sites and not for individual functions
    for (User *U : F.users()) {
      CallInst *CI = dyn_cast<CallInst>(U);
      if (!CI)
        continue;
      if (llvm::isAllocationFn(CI, &tli, true))
        m_allocs.insert(F.getName());
      break;
    }
  }
}

bool AllocWrapInfo::isAllocWrapper(llvm::Function &fn) const {
  if (!m_tliWrapper) {
    llvm::errs() << "ERROR: AllocWrapInfo::initialize must be called\n";
    assert(false);
    return false;
  }    
  return m_allocs.count(fn.getName()) != 0;
}

bool AllocWrapInfo::isDeallocWrapper(llvm::Function &fn) const {
  if (!m_tliWrapper) {
    llvm::errs() << "ERROR: AllocWrapInfo::initialize must be called\n";
    assert(false);
    return false;
  }    
  return m_deallocs.count(fn.getName()) != 0;
}

const std::set<llvm::StringRef> &AllocWrapInfo::getAllocWrapperNames(llvm::Module &M) const {
  if (!m_tliWrapper) {
    llvm::errs() << "ERROR: AllocWrapInfo::initialize must be called\n";
    assert(false);
  }    
  return m_allocs;
}

bool AllocWrapInfo::findWrappers(Module &M, Pass *P,
				 std::set<llvm::StringRef> &fn_names) const {
  // -- copy fn_names since it will change
  std::vector<llvm::StringRef> fns(fn_names.begin(), fn_names.end());

  bool changed = false;
  for (auto &n : fns) {
    Function *F = M.getFunction(n);
    if (!F)
      continue;

    // look through all the calls to F
    for (User *U : F->users()) {
      CallInst *CI = dyn_cast<CallInst>(U);
      if (!CI)
        continue;
      Function *parentFn = CI->getParent()->getParent();
      if (fn_names.count(parentFn->getName()))
        continue;
      if (parentFn->doesNotReturn())
        continue;
      if (!parentFn->getReturnType()->isPointerTy())
        continue;

      // -- possibly a wrapper, do more checks
      bool isWrapper = true;

      LoopInfo *LI = nullptr;
      if (P) {
	// XXX: we would like to use here
	// getAnalysisIfAvailable. However, this method does not take
	// a function as parameter. Instead, all AllocWrapInfo's
	// clients must ensure that if P is not null then
	// LoopInfoWrapperPass is available.
	LI = &(P->getAnalysis<LoopInfoWrapperPass>(*parentFn).getLoopInfo());
      }
      for (auto &bb : *parentFn) {
        ReturnInst *ret = dyn_cast<ReturnInst>(bb.getTerminator());	
        if (ret && !flowsFrom(ret, CI, LI)) {
          isWrapper = false;
          break;
        }
      }
      if (isWrapper)
        isWrapper &= isNotStored(CI);
      if (isWrapper) {
        fn_names.insert(parentFn->getName());
        changed = true;
      }
    }
  }
  return changed;
}

/**
   Returns true if src flows into dst
 */
bool AllocWrapInfo::flowsFrom(Value *dst, Value *src, LoopInfo *LI) const {
  if (dst == src)
    return true;

  else if (ReturnInst *Ret = dyn_cast<ReturnInst>(dst)) {
    return flowsFrom(Ret->getReturnValue(), src, LI);
  }

  else if (PHINode *PN = dyn_cast<PHINode>(dst)) {
    // If this is a loop phi, ignore.
    if (!LI || LI->isLoopHeader(PN->getParent()))
      return false;

    bool ret = true;
    for (Use &v : PN->incoming_values()) {
      ret &= flowsFrom(v.get(), src, LI);
      if (!ret)
        break;
    }
    return ret;
  } else if (BitCastInst *BI = dyn_cast<BitCastInst>(dst)) {
    return flowsFrom(BI->getOperand(0), src, LI);
  } else if (isa<ConstantPointerNull>(dst))
    return true;

  // default
  return false;
}
namespace {
/**
 * Returns true is V does not flow into memory
 */
bool isNotStored(Value *V) {
  for (User *U : V->users()) {
    if (isa<StoreInst>(U))
      return false;
    if (isa<ICmpInst>(U))
      continue;
    if (isa<ReturnInst>(U))
      continue;
    if (BitCastInst *BI = dyn_cast<BitCastInst>(U)) {
      if (isNotStored(BI))
        continue;
      else
        return false;
    }
    if (PHINode *PN = dyn_cast<PHINode>(U)) {
      if (isNotStored(PN))
        continue;
      else
        return false;
    }
    return false;
  }
  return true;
}

} // namespace

char AllocWrapInfo::ID = 0;

using namespace seadsa;
INITIALIZE_PASS_BEGIN(AllocWrapInfo, "seadsa-alloc-wrap-info",
                      "Detects allocator wrappers", false, false)
INITIALIZE_PASS_DEPENDENCY(TargetLibraryInfoWrapperPass)
INITIALIZE_PASS_END(AllocWrapInfo, "seadsa-alloc-wrap-info",
                      "Detects allocator wrappers", false, false)
