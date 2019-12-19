#include "sea_dsa/AllocSiteInfo.hh"

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"

#include "llvm/Analysis/MemoryBuiltins.h"
#include "llvm/Analysis/TargetLibraryInfo.h"

#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Debug.h"

#include "sea_dsa/AllocWrapInfo.hh"
#include "sea_dsa/TypeUtils.hh"
#include "sea_dsa/support/Debug.h"

#define ASI_LOG(...) LOG("alloc_site_info", __VA_ARGS__)
// #define ASI_LOG(...) do { __VA_ARGS__ ; } while (false)

using namespace sea_dsa;
using namespace llvm;

char AllocSiteInfo::ID = 0;

static MDNode *mkMetaConstant(llvm::Optional<unsigned> val, LLVMContext &ctx) {
  MDNode *meta = MDNode::get(ctx, llvm::None);
  if (val.hasValue())
    meta = MDNode::get(ctx, ConstantAsMetadata::get(ConstantInt::get(
                                ctx, llvm::APInt(64u, size_t(*val)))));
  return meta;
}

void AllocSiteInfo::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<TargetLibraryInfoWrapperPass>();
  AU.addRequired<AllocWrapInfo>();
  AU.setPreservesAll();
}

bool AllocSiteInfo::runOnModule(Module &M) {
  m_tli = &getAnalysis<TargetLibraryInfoWrapperPass>().getTLI();
  m_dl = &M.getDataLayout();
  m_awi = &getAnalysis<AllocWrapInfo>();

  bool changed = false;

  // Handle alloc sites inside functions.
  for (auto &fn : M) {
    if (fn.isDeclaration())
      continue;

    ASI_LOG(llvm::errs() << "Running AllocSiteInfo pass on function "
                         << fn.getName() << "\n");
    changed |= markAllocs(fn);
  }

  // Handle global variables with initializers.
  for (auto &gv : M.globals()) {
    if (gv.hasDefinitiveInitializer()) {
      auto sz = maybeEvalAllocSize(gv, M.getContext());
      MDNode *meta = mkMetaConstant(sz, M.getContext());
      gv.setMetadata(m_allocSiteMetadataTag, meta);
      changed = true;
    }
  }

  // TODO(Jakub): what about formals?

  return changed;
}

Optional<unsigned> AllocSiteInfo::maybeEvalAllocSize(Value &v,
                                                     LLVMContext &ctx) {
  llvm::Optional<unsigned> bytes = llvm::None;

  llvm::ObjectSizeOpts Opts;
  Opts.RoundToAlign = true;
  Opts.EvalMode = llvm::ObjectSizeOpts::Mode::Max;
  ObjectSizeOffsetVisitor OSOV(*m_dl, m_tli, ctx, Opts);
  auto OffsetAlign = OSOV.compute(&v);
  if (OSOV.knownSize(OffsetAlign)) {
    const int64_t sz = OffsetAlign.first.getSExtValue();
    assert(sz >= 0);
    bytes = unsigned(sz);
  }

  return bytes;
}

void AllocSiteInfo::markAsAllocSite(Instruction &inst,
                                    Optional<unsigned> allocatedBytes) {
  MDNode *meta = mkMetaConstant(allocatedBytes, inst.getContext());
  inst.setMetadata(m_allocSiteMetadataTag, meta);
}

/// Adds metadata to all known allocating instructions.
/// For allocations of known size, the number of allocated bytes is saved in
/// metadata.
bool AllocSiteInfo::markAllocs(Function &F) {
  bool changed = false;
  for (auto &bb : F)
    for (auto &inst : bb) {
      if (auto *ai = dyn_cast<AllocaInst>(&inst)) {
        unsigned bytes = getTypeSizeInBytes(*ai->getAllocatedType(), *m_dl);
        markAsAllocSite(*ai, bytes);
        changed = true;
        continue;
      }

      if (auto *ci = dyn_cast<CallInst>(&inst)) {
        if (auto *callee = ci->getCalledFunction()) {
          if (m_awi->isAllocWrapper(*callee)) {
            Optional<unsigned> bytes = maybeEvalAllocSize(*ci, F.getContext());
            markAsAllocSite(*ci, bytes);
            changed = true;
          }
        }
      }
    }

  return changed;
}

bool AllocSiteInfo::isAllocSite(const Value &v) {
  if (auto *inst = dyn_cast<Instruction>(&v))
    return inst->getMetadata(getAllocSiteMetadataTag());

  if (auto *gv = dyn_cast<GlobalVariable>(&v))
    return gv->getMetadata(getAllocSiteMetadataTag());

  return false;
}

llvm::Optional<unsigned> AllocSiteInfo::getAllocSiteSize(const Value &v) {
  assert(isAllocSite(v) && "Check if it's an alloc site first!");
  MDNode *meta = nullptr;
  if (auto *inst = dyn_cast<Instruction>(&v))
    meta = inst->getMetadata(getAllocSiteMetadataTag());
  else if (auto *gv = dyn_cast<GlobalVariable>(&v))
    meta = gv->getMetadata(getAllocSiteMetadataTag());
  else
    llvm_unreachable("Wrong value type?");

  assert(meta);

  if (meta->getNumOperands() > 0)
    if (auto *c = dyn_cast<ConstantAsMetadata>(meta->getOperand(0))) {
      Constant *val = c->getValue();
      assert(val);
      assert(isa<ConstantInt>(val));
      auto *valInt = cast<ConstantInt>(val);
      return unsigned(valInt->getLimitedValue());
    }

  return llvm::None;
}

static llvm::RegisterPass<sea_dsa::AllocSiteInfo> X("seadsa-alloc-site-info",
                                                    "Detects allocation sites");
