#include "sea_dsa/FieldType.hh"

#include "llvm/ADT/DenseMap.h"

#include "sea_dsa/Graph.hh"
#include "sea_dsa/TypeUtils.hh"

namespace sea_dsa {

static llvm::cl::opt<bool>
    OmnipotentChar("sea-dsa-omnipotent-char",
                   llvm::cl::desc("Enable SeaDsa omnipotent char"),
                   llvm::cl::init(true));

llvm::Type *GetFirstPrimitiveTy(llvm::Type *Ty) {
  assert(Ty);

  if (Ty->isPointerTy()) {
    auto *ElemTy = Ty->getPointerElementType();
    return GetFirstPrimitiveTy(ElemTy)->getPointerTo(
        Ty->getPointerAddressSpace());
  }

  auto It = AggregateIterator::mkBegin(Ty, /* DL = */ nullptr);
  auto *FirstTy = It->Ty;
  if (FirstTy->isPointerTy() && FirstTy->getPointerElementType() == Ty)
    return Ty;

  if (FirstTy == Ty)
    return FirstTy;

  return GetFirstPrimitiveTy(FirstTy);
}

FieldType::FieldType(llvm::Type *Ty) {
  assert(Ty);

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
  auto *Prim = GetFirstPrimitiveTy(Ty);
  while (Prim->isPointerTy())
    Prim = Prim->getPointerElementType();

  if (auto *ITy = llvm::dyn_cast<llvm::IntegerType>(Prim))
      if (ITy->getBitWidth() == 8)
        return true;

  return false;
}

bool FieldType::IsNotTypeAware() { return !IsTypeAware; }

} // namespace sea_dsa