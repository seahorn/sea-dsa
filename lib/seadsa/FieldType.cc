#include "seadsa/FieldType.hh"

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/Support/CommandLine.h"

#include "seadsa/Graph.hh"
#include "seadsa/TypeUtils.hh"

namespace seadsa {

static llvm::cl::opt<bool> EnableOmnipotentChar(
    "sea-dsa-omnipotent-char",
    llvm::cl::desc("Enable SeaDsa omnipotent char (default is true)"),
    // NOTE: Setting this to false results in unsound results
    // because LLVM insists on storing pointers as i8*
    // even when they have different types in the source
    // language
    llvm::cl::init(true));

namespace seadsa {
bool g_IsTypeAware;
}

namespace {
using SeenTypes = llvm::SmallDenseSet<llvm::Type *, 8>;

llvm::Type *GetInnermostTypeImpl(llvm::Type *const Ty, SeenTypes &seen) {
  assert(Ty);

  static llvm::DenseMap<llvm::Type *, llvm::Type *> s_cachedInnermostTypes;
  {
    auto it = s_cachedInnermostTypes.find(Ty);
    if (it != s_cachedInnermostTypes.end()) return it->getSecond();
  }

  llvm::Type *currentTy = Ty;
  while (!seen.count(currentTy)) {
    seen.insert(currentTy);

    // -- a pointer is a dead-end: opaque pointers carry no pointee to descend
    //    into, so ptr is itself the first primitive type.
    if (currentTy->isPointerTy()) break;

    auto It = AggregateIterator::mkBegin(currentTy, /* DL = */ nullptr);
    auto *FirstTy = It->Ty;
    if (!FirstTy) break;

    // Typed pointers needed an extra stop here for a struct whose first field
    // pointed back at it (`FirstTy->getPointerElementType() == currentTy`),
    // which would otherwise descend forever. Opaque pointers erase the target,
    // so such a struct is plainly { ptr, ... } and the loop already stops at
    // the pointer above -- there is no cycle left to detect.

    if (FirstTy == currentTy) break;

    currentTy = FirstTy;
  }

  s_cachedInnermostTypes.insert({Ty, currentTy});
  return currentTy;
}
} // namespace

/// This is intended to be used within a single llvm::Context. When there's more
/// than one context, the caching might misbehave.
/// LLVM 15: the lack of a pointee type means that pointers are also primitive types, which may be returned.
llvm::Type *GetFirstPrimitiveTy(llvm::Type *const Ty) {
  assert(Ty);

  SeenTypes seen;
  return GetInnermostTypeImpl(Ty, seen);
}

static bool IsOmnipotentChar(llvm::Type *const Ty) {
  assert(Ty);
  if (auto *ITy = llvm::dyn_cast<const llvm::IntegerType>(Ty))
    return ITy->getBitWidth() == 8;
  else if (auto *ATy = llvm::dyn_cast<const llvm::ArrayType>(Ty)) {
    // -- array of omni chars is an omni char
    // -- used by RustC (or llvm optimizer), where [0 x i8]* is used instead of i8*
    return ATy->getNumElements() == 0 && IsOmnipotentChar(ATy->getElementType());
  }

  return false;
}

static bool IsOmnipotentPtr(llvm::Type *const Ty) {
  // No field is omnipotent under opaque pointers, and none needs to be.
  //
  // The omnipotent char existed because LLVM stored a pointer of any source
  // type as i8*, so a link written through an i8* field and one written
  // through, say, an i32* field landed on different Fields for the same
  // offset. Marking the i8* field omni let getLink/addLink bridge them.
  //
  // Opaque pointers collapse every pointer to `ptr`, so two pointer accesses
  // at one offset now build the *same* Field and unify directly. There is no
  // longer a pointee type to test, and nothing left for the test to buy: the
  // omni fallbacks in Graph.cc and Mapper.cc are unreachable by construction.
  return false;
}

FieldType::FieldType(llvm::Type *Ty) {
  assert(Ty);

  // -- debug logging
  static bool s_WarnTypeAware = true;
  if (s_WarnTypeAware && g_IsTypeAware) {
    llvm::errs() << "Sea-Dsa type aware!\n";
    s_WarnTypeAware = false;
  }

  m_ty = IsNotTypeAware() ? nullptr : GetFirstPrimitiveTy(Ty);
  if (m_ty && EnableOmnipotentChar) m_is_omni = IsOmnipotentPtr(m_ty);

  // -- debug logging
  if (m_is_omni) {
    static bool s_shown = false;
    if (!s_shown) {
      llvm::errs() << "Omnipotent char: " << *this << "\n";
      s_shown = true;
    }
  }
}

bool FieldType::IsNotTypeAware() { return !g_IsTypeAware; }

} // namespace seadsa
