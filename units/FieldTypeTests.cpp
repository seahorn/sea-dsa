#include "doctest.h"

#include "seadsa/FieldType.hh"
#include "seadsa/TypeUtils.hh"

#include "llvm/IR/LLVMContext.h"
//#include "llvm/IR/TypeBuilder.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {

LLVMContext &GetCtx() {
  static LLVMContext ctx;
  return ctx;
}

Type *GetInt(LLVMContext &Ctx) {
  return IntegerType::getInt32Ty(Ctx);
}

Type *GetIntPtr(LLVMContext &Ctx) { return GetInt(Ctx)->getPointerTo(0); }

Type *GetVTableTy(LLVMContext &Ctx) {
  return FunctionType::get(GetInt(Ctx), true)->getPointerTo(0);
}

Type *GetVtablePtr(LLVMContext &Ctx) {
  return GetVTableTy(Ctx)->getPointerTo(0);
}

Type *GetFooTy(LLVMContext &Ctx) {
  return StructType::get(
      Ctx, {GetVtablePtr(Ctx), GetIntPtr(Ctx), GetIntPtr(Ctx)}, "Foo");
}

Type *GetFooPtr(LLVMContext &Ctx) { return GetFooTy(Ctx)->getPointerTo(0); }

Type *GetLLTy(LLVMContext &Ctx) {
  static StructType *St = nullptr;
  if (!St) {
    St = StructType::create(Ctx, "LLItem");
    St->setBody({GetIntPtr(Ctx)->getPointerTo(0), St->getPointerTo(0)});
  }
  return St;
}

Type *GetLLPtr(LLVMContext &Ctx) { return GetLLTy(Ctx)->getPointerTo(0); }

} // namespace

TEST_CASE("FirstPrimT.Identity") {
  LLVMContext &C = GetCtx();
  auto *IntPtrTy = GetIntPtr(C);
  CHECK(seadsa::GetFirstPrimitiveTy(IntPtrTy) == GetIntPtr(C));
}

TEST_CASE("FirstPrimT.Scalar") {
  LLVMContext &C = GetCtx();
  auto *IntTy = GetInt(C);
  CHECK(seadsa::GetFirstPrimitiveTy(IntTy) == IntTy);
}

TEST_CASE("FirstPrimT.PtrToScalar") {
  LLVMContext &C = GetCtx();
  auto *IntPtr = GetIntPtr(C);
  CHECK(seadsa::GetFirstPrimitiveTy(IntPtr) == IntPtr);
}

TEST_CASE("FirstPrimT.Foo") {
  LLVMContext &C = GetCtx();
  auto *FooTy = GetFooTy(C);
  CHECK(seadsa::GetFirstPrimitiveTy(FooTy) == GetVtablePtr(C));
  CHECK(seadsa::GetFirstPrimitiveTy(FooTy->getPointerTo(0)) ==
        GetVtablePtr(C)->getPointerTo(0));
}

TEST_CASE("FirstPrimT.LL") {
  LLVMContext &C = GetCtx();
  auto *LLTy = GetLLTy(C);
  CHECK(seadsa::GetFirstPrimitiveTy(LLTy) == GetIntPtr(C)->getPointerTo(0));
}

TEST_CASE("FirstPrimT.LL_reverse") {
  LLVMContext &C = GetCtx();
  auto *St = StructType::create(C, "LL2");
  St->setBody({St->getPointerTo(0), GetIntPtr(C)->getPointerTo(0)});
  CHECK(seadsa::GetFirstPrimitiveTy(St) == St);
}

TEST_CASE("FirstPrimT.Nested") {
  LLVMContext &C = GetCtx();
  auto *St = StructType::create(C, "Nested");
  St->setBody({GetFooPtr(C), GetIntPtr(C)->getPointerTo(0)});
  CHECK(seadsa::GetFirstPrimitiveTy(St) == GetVtablePtr(C)->getPointerTo(0));
  CHECK(seadsa::GetFirstPrimitiveTy(St)->getPointerTo(0) ==
        GetVtablePtr(C)->getPointerTo(0)->getPointerTo(0));
}

// Make sure the caching in GetFirstPrimitiveTy doesn't misbehave.
TEST_CASE("FirstPrimT.Cache") {
  LLVMContext &C = GetCtx();
  auto *LLTy = GetLLTy(C);
  CHECK(seadsa::GetFirstPrimitiveTy(LLTy) == GetIntPtr(C)->getPointerTo(0));
  CHECK(seadsa::GetFirstPrimitiveTy(LLTy) == GetIntPtr(C)->getPointerTo(0));
}
