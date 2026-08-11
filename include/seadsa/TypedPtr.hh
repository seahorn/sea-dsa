#pragma once

#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Type.h"

namespace seadsa {

class TypedPointerType : public llvm::Type {
  explicit TypedPointerType(Type *PointeeTy, unsigned AddrSpace);

  Type *PointeeTy;

public:
  TypedPointerType(const TypedPointerType &) = delete;
  TypedPointerType &operator=(const TypedPointerType &) = delete;

  // This constructs a pointer to an object of the specified type in a numbered
  // address space.
  static TypedPointerType *get(Type *ElementType, unsigned AddressSpace);

  // This constructs a pointer to an object of the specified type in the default
  // address space (address space zero).
  static TypedPointerType *getUnqual(Type *ElementType);

  // This constructs a pointer type with the same pointee type as input
  // TypedPointerType and the given address space.
  static TypedPointerType *getWithSamePointeeType(TypedPointerType *PT,
                                                  unsigned AddressSpace);

  // TypedPointerType is never opaque.
  bool isOpaque() const;

  // Return true if the specified type is valid as a element type.
  static bool isValidElementType(Type *ElemTy);

  // Return true if we can load or store from a pointer to this type.
  static bool isLoadableOrStorableType(Type *ElemTy);

  // Return the address space of the pointer type.
  inline unsigned getAddressSpace() const { return getSubclassData(); }

  // Return true if either this is an opaque pointer type or if this pointee
  // type matches Ty.
  bool isOpaqueOrPointeeTypeMatches(Type *Ty);

  // Return true if both pointer types have the same element type.
  bool hasSameElementTypeAs(const TypedPointerType *Other);

  // Implement support type inquiry through isa, cast, and dyn_cast.
  static bool classof(const Type *T);

  // Return the pointee type.
  Type *getPointeeType() const;

  // Return the corresponding opaque PointerType.
  llvm::PointerType *getOpaquePointerType() const;

  static TypedPointerType *getTypedPointerTo(Type *ty, unsigned AddrSpace);

  // Skeleton: Infer a TypedPointerType from an opaque PointerType.
  static TypedPointerType *inferFromOpaquePointer(const llvm::PointerType *PTy);

  // --- Methods that delegate to the opaque pointer ---
  // Example: If PointerType had a method foo(), delegate as:
  // auto foo() const { return getOpaquePointerType()->foo(); }
  // Add more delegations as needed to match PointerType's API.
};

} // namespace seadsa
