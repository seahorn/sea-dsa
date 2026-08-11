#include "seadsa/TypedPtr.hh"

#include <cassert>

using namespace seadsa;

TypedPointerType::TypedPointerType(Type *PointeeTy, unsigned AddrSpace)
    : Type(PointeeTy->getContext(), PointerTyID), PointeeTy(PointeeTy) {
  assert(PointeeTy && "TypedPointerType must have a non-null pointee type!");
  setSubclassData(AddrSpace);
  ContainedTys = &this->PointeeTy;
  NumContainedTys = 1;
}

TypedPointerType *TypedPointerType::get(Type *ElementType,
                                        unsigned AddressSpace) {
  assert(ElementType && "Can't get a pointer to <null> type!");
  assert(isValidElementType(ElementType) &&
         "Invalid type for pointer element!");
  // For now, just create a new instance every time.
  return new TypedPointerType(ElementType, AddressSpace);
}

TypedPointerType *TypedPointerType::getUnqual(Type *ElementType) {
  return get(ElementType, 0);
}

TypedPointerType *TypedPointerType::getWithSamePointeeType(
    TypedPointerType *PT, unsigned AddressSpace) {
  return get(PT->getPointeeType(), AddressSpace);
}

bool TypedPointerType::isOpaque() const {
  return false;
}

bool TypedPointerType::isValidElementType(Type *ElemTy) {
  return llvm::PointerType::isValidElementType(ElemTy);
}

bool TypedPointerType::isLoadableOrStorableType(Type *ElemTy) {
  return llvm::PointerType::isLoadableOrStorableType(ElemTy);
}

bool TypedPointerType::isOpaqueOrPointeeTypeMatches(Type *Ty) {
  // Always non-opaque, so just compare types.
  return PointeeTy == Ty;
}

bool TypedPointerType::hasSameElementTypeAs(const TypedPointerType *Other) {
  return PointeeTy == Other->PointeeTy;
}

bool TypedPointerType::classof(const Type *T) {
  return T->getTypeID() == PointerTyID;
}

llvm::Type *TypedPointerType::getPointeeType() const {
  assert(PointeeTy && "TypedPointerType must have a non-null pointee type!");
  return PointeeTy;
}

llvm::PointerType *TypedPointerType::getOpaquePointerType() const {
  return llvm::PointerType::get(PointeeTy->getContext(), getAddressSpace());
}

TypedPointerType *TypedPointerType::getTypedPointerTo(Type *ty, unsigned AddrSpace) {
  return TypedPointerType::get(ty, AddrSpace);
}

TypedPointerType *
TypedPointerType::inferFromOpaquePointer(const llvm::PointerType *PTy) {
  // TODO: Implement inference from opaque pointer type to typed pointer type.
  // This is a skeleton for user implementation.
  return nullptr;
}