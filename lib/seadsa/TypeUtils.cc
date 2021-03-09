#include "seadsa/TypeUtils.hh"

#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Type.h"

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"

#include <algorithm>

using namespace llvm;

unsigned seadsa::getTypeSizeInBytes(const Type &ty, const DataLayout &dl) {
  return dl.getTypeStoreSize(&const_cast<Type &>(ty));
}

namespace seadsa {

void SubTypeDesc::dump(llvm::raw_ostream &OS /* = llvm::errs() */) {
  OS << "SubTypeDesc:  ";
  if (Ty)
    Ty->print(OS);
  else
    OS << "nullptr";

  OS << "\n\tbytes:  " << Bytes << "\n\toffset:  " << Offset << "\n";
}

unsigned AggregateIterator::sizeInBytes(Type *Ty) const {
  // Don't track offsets when DL is not available.
  if (!DL)
    return 0;

  return getTypeSizeInBytes(*Ty, *DL);
}

void AggregateIterator::doStep() {
  if (Worklist.empty()) {
    const auto NewOffset = Current.Offset + Current.Bytes;
    Current = {nullptr, 0, NewOffset};
    return;
  }

  while (!Worklist.empty()) {
    auto *const Ty = Worklist.pop_back_val();

    // Shaobo: `CompositeType` is removed in LLVM 11.
    // Previously, it mounts to the union of ArrayTy, StructTy, and VectorTy.
    if (Ty->isPointerTy() || !(Ty->isAggregateType() || Ty->isVectorTy())) {
      const auto NewOffset = Current.Offset + Current.Bytes;
      Current = SubTypeDesc{Ty, sizeInBytes(Ty), NewOffset};
      break;
    }

    if (Ty->isArrayTy()) {
      for (size_t i = 0, e = Ty->getArrayNumElements(); i != e; ++i)
        Worklist.push_back(Ty->getArrayElementType());
      continue;
    }

    if (Ty->isVectorTy()) {
      // Shaobo: LLVM 11 introduced `ScalableVectorType` whose number of
      // elements may not be fixed. So I used `FixedVectorType` here.
      assert(llvm::isa<FixedVectorType>(Ty) &&
             "ScalableVectorType is not supported!");
      auto fvt = llvm::cast<FixedVectorType>(Ty);
      for (size_t i = 0, e = fvt->getNumElements(); i != e; ++i)
        Worklist.push_back(fvt->getElementType());
      continue;
    }

    assert(Ty->isStructTy());
    for (auto *SubTy : llvm::reverse(Ty->subtypes()))
      Worklist.push_back(SubTy);
  }
}
} // namespace seadsa
