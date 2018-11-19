#ifndef SEA_DSA_FIELD_TYPE
#define SEA_DSA_FIELD_TYPE

#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/raw_ostream.h"

#include <string>
#include <tuple>

namespace sea_dsa {

llvm::Type *GetFirstPrimitiveTy(llvm::Type *Ty);

#define FIELD_TYPE_STRINGIFY(X) #X
#define FIELD_TYPE_NOT_IMPLEMENTED sea_dsa::FieldType::NotImplemented( \
                                                FIELD_TYPE_STRINGIFY(__LINE__ ))

class FieldType {
  llvm::Type *m_ty = nullptr;
  bool m_NOT_IMPLEMENTED = false;
  const char *m_whereNotImpl = "";

  FieldType() = default;

public:
  std::tuple<llvm::Type *, bool> asTuple() const {
    return std::make_tuple(m_ty, m_NOT_IMPLEMENTED);
  };

  static FieldType mkUnknown() { return FieldType(); }

  static FieldType NotImplemented(const char *Loc) {
    FieldType ft;
    ft.m_NOT_IMPLEMENTED = true;
    ft.m_whereNotImpl = Loc;
    return ft;
  }

  explicit FieldType(llvm::Type *Ty);

  FieldType(const FieldType &ft) = default;
  FieldType &operator=(const FieldType &) = default;

  FieldType(FieldType &&) = default;
  FieldType &operator=(FieldType &&) = default;

  bool isData() const { return !m_ty->isPointerTy(); }
  bool isPointer() const { return m_ty->isPointerTy(); }
  bool isUnknown() const { return !m_ty; }

  static bool IsOmnipotentChar(llvm::Type *Ty);

  llvm::Type *getLLVMType() const { return m_ty; }

  bool operator==(const FieldType &RHS) const {
    if (isUnknown() || RHS.isUnknown() || IsNotTypeAware())
      return true;

    return asTuple() == RHS.asTuple();
  }

  // opaque is top.
  bool operator<(const FieldType &RHS) const {
    //assert(isUnknown());
    //assert(RHS.isUnknown());

    if (isUnknown() || RHS.isUnknown() || IsNotTypeAware())
      return false;

    return asTuple() < RHS.asTuple();
  }

  FieldType ptrOf() const {
    assert(!isUnknown());
    auto *PtrTy = llvm::PointerType::get(m_ty, 0);
    return FieldType{PtrTy};
  }

  FieldType elemOf() const {
    assert(!isUnknown());
    assert(isPointer());

    auto *NewTy = m_ty->getPointerElementType();
    return FieldType{NewTy};
  }

  void dump(llvm::raw_ostream &OS = llvm::errs()) const {
    if (m_NOT_IMPLEMENTED) {
      OS << "TODO<" << m_whereNotImpl << ">";
      return;
    }

    if (isUnknown()) {
      OS << "unknown";
      return;
    }

    if (m_ty->isStructTy())
      OS << '%' << m_ty->getStructName();
    else
      m_ty->print(OS, false);
  }

  friend llvm::raw_ostream &operator<<(llvm::raw_ostream &OS,
                                       const FieldType &FTy) {
    FTy.dump(OS);
    return OS;
  }

private:
  static bool IsNotTypeAware();
};

} // namespace sea_dsa

#endif // SEA_DSA_FIELD_TYPE