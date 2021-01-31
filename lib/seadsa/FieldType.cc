#include "seadsa/FieldType.hh"

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/Support/CommandLine.h"

#include "seadsa/Graph.hh"
#include "seadsa/TypeUtils.hh"

namespace seadsa {

static llvm::cl::opt<bool>
    EnableOmnipotentChar("sea-dsa-omnipotent-char",
                         llvm::cl::desc("Enable SeaDsa omnipotent char (default is true)"),
                         // NOTE: Setting this to false results in unsound results
                         // because LLVM insists on storing pointers as i8*
                         // even when they have different types in the source
                         // language
                         llvm::cl::init(true));

namespace seadsa {
bool g_IsTypeAware;
}

namespace {
using SeenTypes = llvm::SmallDenseSet<llvm::Type *, 8>;

llvm::Type *GetInnermostTypeImpl(llvm::Type *const Ty, SeenTypes &seen) {
  assert(Ty);

  static llvm::DenseMap<llvm::Type *, llvm::Type *> s_cachedInnermostTypes;
  {
    auto it = s_cachedInnermostTypes.find(Ty);
    if (it != s_cachedInnermostTypes.end()) return it->getSecond();
  }

  llvm::Type *currentTy = Ty;
  while (!seen.count(currentTy)) {
    seen.insert(currentTy);

    if (currentTy->isPointerTy()) {
      auto *ElemTy = currentTy->getPointerElementType();
      currentTy = GetInnermostTypeImpl(ElemTy, seen)
                      ->getPointerTo(currentTy->getPointerAddressSpace());
      break;
    }

    auto It = AggregateIterator::mkBegin(currentTy, /* DL = */ nullptr);
    auto *FirstTy = It->Ty;
    if (!FirstTy) break;

    if (FirstTy->isPointerTy() && FirstTy->getPointerElementType() == currentTy)
      break;

    if (FirstTy == currentTy) break;

    currentTy = FirstTy;
  }

  s_cachedInnermostTypes.insert({Ty, currentTy});
  return currentTy;
}
} // namespace

/// This is intended to be used within a single llvm::Context. When there's more
/// than one context, the caching might misbehave.
llvm::Type *GetFirstPrimitiveTy(llvm::Type *const Ty) {
  assert(Ty);

  SeenTypes seen;
  return GetInnermostTypeImpl(Ty, seen);
}

static bool IsOmnipotentChar(llvm::Type *const Ty) {
  assert(Ty);
  if (auto *ITy = llvm::dyn_cast<const llvm::IntegerType>(Ty))
    return ITy->getBitWidth() == 8;

  return false;
}

static bool IsOmnipotentPtr(llvm::Type *const Ty) {
  auto res = Ty->isPointerTy() && IsOmnipotentChar(Ty->getPointerElementType());
  return res;
}

FieldType::FieldType(llvm::Type *Ty) {
  assert(Ty);

  // -- debug logging
  static bool s_WarnTypeAware = true;
  if (s_WarnTypeAware && g_IsTypeAware) {
    llvm::errs() << "Sea-Dsa type aware!\n";
    s_WarnTypeAware = false;
  }

  m_ty = IsNotTypeAware() ? nullptr : GetFirstPrimitiveTy(Ty);
  if (m_ty && EnableOmnipotentChar) m_is_omni = IsOmnipotentPtr(m_ty);

  // -- debug logging
  if (m_is_omni) {
    static bool s_shown = false;
    if (!s_shown) {
      llvm::errs() << "Omnipotent char: " << *this << "\n";
      s_shown = true;
    }
  }
}

bool FieldType::IsNotTypeAware() { return !g_IsTypeAware; }

} // namespace seadsa
