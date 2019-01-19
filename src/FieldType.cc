#include "sea_dsa/FieldType.hh"

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"

#include "sea_dsa/Graph.hh"
#include "sea_dsa/TypeUtils.hh"

namespace sea_dsa {

static llvm::cl::opt<bool>
    OmnipotentChar("sea-dsa-omnipotent-char",
                   llvm::cl::desc("Enable SeaDsa omnipotent char"),
                   llvm::cl::init(false));

namespace sea_dsa {
  bool IsTypeAware;
}

namespace {
using SeenTypes = llvm::SmallDenseSet<llvm::Type *, 8>;

llvm::Type *GetInnermostTypeImpl(llvm::Type *const Ty, SeenTypes &seen) {
  assert(Ty);

  static llvm::DenseMap<llvm::Type *, llvm::Type *> cachedInnermostTypes;
  if (cachedInnermostTypes.count(Ty) > 0)
    return cachedInnermostTypes[Ty];

  llvm::Type *currentTy = Ty;
  while (seen.count(currentTy) == 0) {
    seen.insert(currentTy);

    if (currentTy->isPointerTy()) {
      auto *ElemTy = currentTy->getPointerElementType();
      currentTy = GetInnermostTypeImpl(ElemTy, seen)->getPointerTo(
          currentTy->getPointerAddressSpace());
      break;
    }

    auto It = AggregateIterator::mkBegin(currentTy, /* DL = */ nullptr);
    auto *FirstTy = It->Ty;
    if (!FirstTy)
      break;

    if (FirstTy->isPointerTy() && FirstTy->getPointerElementType() == currentTy)
      break;

    if (FirstTy == currentTy)
      break;

    currentTy = FirstTy;
  }

  cachedInnermostTypes[Ty] = currentTy;
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

FieldType::FieldType(llvm::Type *Ty) {
  assert(Ty);

  static bool WarnTypeAware = true;
  if (WarnTypeAware && IsTypeAware) {
    llvm::errs() << "Sea-Dsa type aware!\n";
    WarnTypeAware = false;
  }

  if (!IsTypeAware) {
    m_ty = nullptr;
    return;
  }

  m_ty = GetFirstPrimitiveTy(Ty);
  if (OmnipotentChar && IsOmnipotentChar(m_ty)) {
    static bool shown = false;
    if (!shown) {
      llvm::errs() << "Omnipotent char: " << *this << "\n";
      shown = true;
    }
    m_ty = nullptr;
  }
}

bool FieldType::IsOmnipotentChar(llvm::Type *Ty) {
  assert(Ty);
  if (auto *ITy = llvm::dyn_cast<llvm::IntegerType>(Ty))
      if (ITy->getBitWidth() == 8)
        return true;

  return false;
}

bool FieldType::IsNotTypeAware() { return !IsTypeAware; }

} // namespace sea_dsa
