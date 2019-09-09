#ifndef SEA_DSA_TYPE_UTILS
#define SEA_DSA_TYPE_UTILS

#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/raw_ostream.h"

namespace llvm {
class DataLayout;
class Type;
} // namespace llvm

namespace sea_dsa {

unsigned getTypeSizeInBytes(const llvm::Type &ty, const llvm::DataLayout &dl);

struct SubTypeDesc {
  llvm::Type *Ty = nullptr;
  unsigned Bytes = 0;
  unsigned Offset = 0;

  SubTypeDesc() = default;
  SubTypeDesc(llvm::Type *Ty_, unsigned Bytes_, unsigned Offset_)
      : Ty(Ty_), Bytes(Bytes_), Offset(Offset_) {}

  void dump(llvm::raw_ostream &OS = llvm::errs());
};

class AggregateIterator {
  const llvm::DataLayout *DL;
  llvm::SmallVector<llvm::Type *, 8> Worklist;
  SubTypeDesc Current;

  AggregateIterator(const llvm::DataLayout *DL_, llvm::Type *Ty_) : DL(DL_) {
    Worklist.push_back({Ty_});
  }

public:
  static AggregateIterator mkBegin(llvm::Type *Ty, const llvm::DataLayout *DL) {
    AggregateIterator Res(DL, Ty);
    Res.doStep();
    return Res;
  }

  static AggregateIterator mkEnd(llvm::Type *Ty, const llvm::DataLayout *DL) {
    AggregateIterator Res(DL, nullptr);
    Res.Worklist.clear();
    return Res;
  }

  static llvm::iterator_range<AggregateIterator>
  range(llvm::Type *Ty, const llvm::DataLayout *DL) {
    return llvm::make_range(mkBegin(Ty, DL), mkEnd(Ty, DL));
  }

  AggregateIterator &operator++() {
    doStep();
    return *this;
  }

  SubTypeDesc &operator*() { return Current; }
  SubTypeDesc *operator->() { return &Current; }

  bool operator!=(const AggregateIterator &Other) const {
    return Current.Ty != Other.Current.Ty ||
           Current.Bytes != Other.Current.Bytes || Worklist != Other.Worklist;
  }

  unsigned sizeInBytes(llvm::Type *Ty) const;

private:
  void doStep();
};

} // namespace sea_dsa

#endif // SEA_DSA_TYPE_UTILS
