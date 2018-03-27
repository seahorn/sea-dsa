#ifndef SEA_DSA_FIELD_TYPE
#define SEA_DSA_FIELD_TYPE

#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Type.h"

namespace sea_dsa {

llvm::Type *GetFirstPrimitiveTy(llvm::Type *Ty);

class FieldType {
public:
  llvm::Type *ty = nullptr;
  bool isOpaque = false;

  bool isData() const { return !ty->isPointerTy(); }

  bool isPointer() const { return ty->isPointerTy(); }

  FieldType() = default;

  FieldType(llvm::Type *Ty, bool IsOpaque = false) {
    ty = GetFirstPrimitiveTy(Ty);
    isOpaque = IsOpaque;
  }

  FieldType(const FieldType &) = default;

  FieldType &operator=(const FieldType &) = default;

  bool operator==(const FieldType &RHS) const {
    return ty == RHS.ty && isOpaque == RHS.isOpaque;
  }

  FieldType ptrOf() const {
    assert(!isOpaque);
    auto *PtrTy = llvm::PointerType::get(ty, 0);
    return FieldType{PtrTy, false};
  }

  FieldType elemOf() const {
    assert(!isOpaque);
    assert(isPointer());

    auto *NewTy = ty->getPointerElementType();
    return FieldType{NewTy, false};
  }
};

} // namespace sea_dsa

#endif // SEA_DSA_FIELD_TYPE