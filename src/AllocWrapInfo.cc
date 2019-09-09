#include "sea_dsa/AllocWrapInfo.hh"

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/MemoryBuiltins.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"

#include <vector>

using namespace sea_dsa;
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

char AllocWrapInfo::ID = 0;

void AllocWrapInfo::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<TargetLibraryInfoWrapperPass>();
  AU.addRequired<LoopInfoWrapperPass>();
  AU.setPreservesAll();
}

bool AllocWrapInfo::runOnModule(Module &M) {
  m_tli = &getAnalysis<TargetLibraryInfoWrapperPass>().getTLI();

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
  while (findWrappers(M, m_allocs)) { /* skip */
    ;
  }
  while (findWrappers(M, m_deallocs)) { /* skip */
    ;
  }

  return false;
}

void AllocWrapInfo::findAllocs(Module &M) {
  for (Function &F : M) {
    if (m_allocs.count(F.getName()))
      continue;
    // find the first call because MemoryBuiltins only provides
    // interface for call-sites and not for individual functions
    for (User *U : F.users()) {
      CallInst *CI = dyn_cast<CallInst>(U);
      if (!CI)
        continue;
      if (llvm::isAllocationFn(CI, &getTLI(), true))
        m_allocs.insert(F.getName());
      break;
    }
  }
}

bool AllocWrapInfo::isAllocWrapper(llvm::Function &fn) const {
  return m_allocs.count(fn.getName()) != 0;
}

bool AllocWrapInfo::isDeallocWrapper(llvm::Function &fn) const {
  return m_deallocs.count(fn.getName()) != 0;
}

bool AllocWrapInfo::findWrappers(Module &M, std::set<std::string> &fn_names) {
  // -- copy fn_names since it will change
  std::vector<std::string> fns(fn_names.begin(), fn_names.end());

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

      for (auto &bb : *parentFn) {
        ReturnInst *ret = dyn_cast<ReturnInst>(bb.getTerminator());
        if (ret && !flowsFrom(ret, CI)) {
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
bool AllocWrapInfo::flowsFrom(Value *dst, Value *src) {
  if (dst == src)
    return true;

  else if (ReturnInst *Ret = dyn_cast<ReturnInst>(dst)) {
    return flowsFrom(Ret->getReturnValue(), src);
  }

  else if (PHINode *PN = dyn_cast<PHINode>(dst)) {
    Function *F = PN->getParent()->getParent();
    LoopInfo &LI = getAnalysis<LoopInfoWrapperPass>(*F).getLoopInfo();
    // If this is a loop phi, ignore.
    if (LI.isLoopHeader(PN->getParent()))
      return false;

    bool ret = true;
    for (Use &v : PN->incoming_values()) {
      ret &= flowsFrom(v.get(), src);
      if (!ret)
        break;
    }
    return ret;
  } else if (BitCastInst *BI = dyn_cast<BitCastInst>(dst)) {
    return flowsFrom(BI->getOperand(0), src);
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

static llvm::RegisterPass<sea_dsa::AllocWrapInfo>
    X("seadsa-alloc-wrap-info",
      "Detects allocator wrappers");
