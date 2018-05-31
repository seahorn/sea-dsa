#include "sea_dsa/FieldType.hh"

#include "llvm/ADT/DenseMap.h"

#include "sea_dsa/TypeUtils.hh"

namespace sea_dsa {

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

bool FieldType::IsOmnipotentChar(llvm::Type *Ty) {
  assert(Ty);
  auto *Prim = GetFirstPrimitiveTy(Ty);
  if (auto *PTy = llvm::dyn_cast<llvm::PointerType>(Prim))
    if (auto *ITy =
        llvm::dyn_cast<llvm::IntegerType>(PTy->getPointerElementType()))
      if (ITy->getBitWidth() == 8)
        return true;

  return false;
}

} // namespace sea_dsa