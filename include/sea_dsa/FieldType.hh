#ifndef SEA_DSA_FIELD_TYPE
#define SEA_DSA_FIELD_TYPE

#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/raw_ostream.h"

namespace sea_dsa {

llvm::Type *GetFirstPrimitiveTy(llvm::Type *Ty);

class FieldType {
public:
  llvm::Type *ty = nullptr;
  bool isOpaque = false;

  FieldType() = default;

  FieldType(llvm::Type *Ty, bool IsOpaque = false) : isOpaque(IsOpaque) {
    if (Ty)
      ty = GetFirstPrimitiveTy(Ty);
  }

  FieldType(const FieldType &) = default;
  FieldType &operator=(const FieldType &) = default;

  bool isData() const { return !ty->isPointerTy(); }
  bool isPointer() const { return ty->isPointerTy(); }
  bool isNull() const { return !ty; }

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

  void dump(llvm::raw_ostream &OS = llvm::errs()) const {
    if (isOpaque) {
      OS << "opaque";
      return;
    }

    if (!ty) {
      OS << "null";
      return;
    }

    ty->print(OS, false);
  }

  friend llvm::raw_ostream &operator<<(llvm::raw_ostream &OS,
                                       const FieldType &FTy) {
    FTy.dump(OS);
    return OS;
  }
};

} // namespace sea_dsa

#endif // SEA_DSA_FIELD_TYPE