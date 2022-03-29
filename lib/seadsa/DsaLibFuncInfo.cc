
#include "llvm/ADT/Twine.h"
#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Pass.h"
#include "llvm/PassRegistry.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/Registry.h"
#include "llvm/Support/SourceMgr.h"

#include "seadsa/DsaLibFuncInfo.hh"
#include "seadsa/Graph.hh"
#include "seadsa/InitializePasses.hh"
#include "seadsa/support/Debug.h"
#include <fstream>
#include <iostream>
#include <unordered_set>

static llvm::cl::list<std::string>
    XSpecFiles("sea-dsa-specfile", llvm::cl::desc("<Input spec bitcode file>"),
               llvm::cl::ZeroOrMore, llvm::cl::value_desc("filename"));

static llvm::cl::opt<bool> XUseClibSpec(
    "sea-dsa-use-clib-spec",
    llvm::cl::desc("Replace clib functions with spec defined by seadsa"),
    llvm::cl::Optional, llvm::cl::init(false));

static llvm::cl::opt<std::string>
    XGenSpecs("sea-dsa-gen-specs", llvm::cl::desc("<Spec output file name>"),
              llvm::cl::Optional, llvm::cl::value_desc("filename"));

namespace seadsa {

void DsaLibFuncInfo::getAnalysisUsage(llvm::AnalysisUsage &AU) const {
  AU.setPreservesAll();
}

void DsaLibFuncInfo::initSpecModule() const {
  auto specFnModify = m_specLang->getFunction("sea_dsa_set_modified");
  auto specFnRead = m_specLang->getFunction("sea_dsa_set_read");
  auto specFnCollapse = m_specLang->getFunction("sea_dsa_collapse");
  auto specFnHeap = m_specLang->getFunction("sea_dsa_set_heap");
  auto specFnP2I = m_specLang->getFunction("sea_dsa_set_ptrtoint");
  auto specFnI2P = m_specLang->getFunction("sea_dsa_set_inttoptr");
  auto specFnExtern = m_specLang->getFunction("sea_dsa_set_external");
  auto specFnAlias = m_specLang->getFunction("sea_dsa_alias");
  auto specFnMk = m_specLang->getFunction("sea_dsa_mk");
  auto specFnLink = m_specLang->getFunction("sea_dsa_link");
  auto specFnAccess = m_specLang->getFunction("sea_dsa_access");

  assert(specFnModify != nullptr && "Could not find sea_dsa_set_modified");
  assert(specFnRead != nullptr && "Could not find sea_dsa_set_read");
  assert(specFnCollapse != nullptr && "Could not find sea_dsa_set_heap");
  assert(specFnHeap != nullptr && "Could not find sea_dsa_collapse");
  assert(specFnP2I != nullptr && "Could not find sea_dsa_set_ptrtoint");
  assert(specFnI2P != nullptr && "Could not find sea_dsa_set_inttoptr");
  assert(specFnExtern != nullptr && "Could not find sea_dsa_set_external");
  assert(specFnAlias != nullptr && "Could not find sea_dsa_alias");
  assert(specFnMk != nullptr && "Could not find sea_dsa_mk");
  assert(specFnLink != nullptr && "Could not find sea_dsa_link");
  assert(specFnAccess != nullptr && "Could not find sea_dsa_access");

  m_specModule->getOrInsertFunction(specFnModify->getName(),
                                    specFnModify->getFunctionType());
  m_specModule->getOrInsertFunction(specFnRead->getName(),
                                    specFnRead->getFunctionType());
  m_specModule->getOrInsertFunction(specFnCollapse->getName(),
                                    specFnCollapse->getFunctionType());
  m_specModule->getOrInsertFunction(specFnHeap->getName(),
                                    specFnHeap->getFunctionType());
  m_specModule->getOrInsertFunction(specFnP2I->getName(),
                                    specFnP2I->getFunctionType());
  m_specModule->getOrInsertFunction(specFnI2P->getName(),
                                    specFnI2P->getFunctionType());
  m_specModule->getOrInsertFunction(specFnExtern->getName(),
                                    specFnExtern->getFunctionType());
  m_specModule->getOrInsertFunction(specFnAlias->getName(),
                                    specFnAlias->getFunctionType());
  m_specModule->getOrInsertFunction(specFnMk->getName(),
                                    specFnMk->getFunctionType());
  m_specModule->getOrInsertFunction(specFnLink->getName(),
                                    specFnLink->getFunctionType());
  m_specModule->getOrInsertFunction(specFnAccess->getName(),
                                    specFnAccess->getFunctionType());
}

void DsaLibFuncInfo::initialize(const llvm::Module &M) const {

  using namespace llvm::sys::fs;
  using namespace llvm;

  if (m_isInitialized ||
      (XSpecFiles.empty() && !XUseClibSpec && XGenSpecs.empty()))
    return;
  m_isInitialized = true;

  SMDiagnostic err;
  auto &dl = M.getDataLayout();
  auto &ctx = M.getContext();

  auto exePath = getMainExecutable(nullptr, nullptr);
  StringRef exeDir = llvm::sys::path::parent_path(exePath);

  const StringRef specLangRel = "../lib/sea_dsa.ll";
  llvm::SmallString<256> specLangAbs = specLangRel;
  make_absolute(exeDir, specLangAbs);
  m_specLang = parseIRFile(specLangAbs.str(), err, ctx);

  if (m_specLang.get() == 0)
    LOG("sea-libFunc", errs() << "Error reading sea_dsa.h bitcode: "
                              << err.getMessage() << "\n");

  if (XUseClibSpec) {
    if (dl.getPointerSizeInBits() == 32) {
      const StringRef libcSpecRel = "../lib/libc-32.spec.bc";
      llvm::SmallString<256> libcSpecAbs = libcSpecRel;
      make_absolute(exeDir, libcSpecAbs);

      ModuleRef specM = parseIRFile(libcSpecAbs.str(), err, M.getContext());
      if (specM.get() == 0)
        LOG("sea-libFunc", errs() << "Error reading Clib spec file: "
                                  << err.getMessage() << "\n");
      else
        m_modules.push_back(std::move(specM));
    } else {
      const StringRef libcSpecRel = "../lib/libc-64.spec.bc";
      llvm::SmallString<256> libcSpecAbs = libcSpecRel;
      make_absolute(exeDir, libcSpecAbs);

      ModuleRef specM = parseIRFile(libcSpecAbs.str(), err, M.getContext());
      if (specM.get() == 0)
        LOG("sea-libFunc", errs() << "Error reading Clib spec file: "
                                  << err.getMessage() << "\n");
      else
        m_modules.push_back(std::move(specM));
    }

    if (!m_modules.empty()) {
      auto &clib_funcs = m_modules.back()->getFunctionList();
      for (llvm::Function &func : clib_funcs) {
        if (func.isDeclaration() || func.empty()) continue;
        m_funcs.insert({func.getName(), &func});
      }
    }
  }

  for (auto &specFile : XSpecFiles) {
    ModuleRef specM = parseIRFile(specFile, err, M.getContext());
    if (specM.get() == 0) {
      LOG("sea-libFunc", errs() << "Error reading" << specFile << ": "
                                << err.getMessage() << "\n");
    } else {
      m_modules.push_back(std::move(specM));

      auto &spec_funcs = m_modules.back()->getFunctionList();
      for (llvm::Function &func : spec_funcs) {
        if (func.isDeclaration() || func.empty()) continue;
        m_funcs.insert({func.getName(), &func});
      }
    }
  }

  if (!XGenSpecs.empty()) {
    m_genSpecOpt = true;
    m_specModule = std::make_unique<Module>(M.getName(), ctx);
    m_specModule->setDataLayout(dl);
    m_specModule->setSourceFileName(XGenSpecs);
    initSpecModule();
  }
} // namespace seadsa

bool DsaLibFuncInfo::hasSpecFunc(const llvm::Function &F) const {
  return m_funcs.count(F.getName());
}

llvm::Function *DsaLibFuncInfo::getSpecFunc(const llvm::Function &F) const {
  auto it = m_funcs.find(F.getName());
  assert(it != m_funcs.end());
  return it->second;
}

void DsaLibFuncInfo::generateSpec(const llvm::Function &F,
                                  const GraphRef G) const {
  using namespace llvm;

  auto specFnModify = m_specModule->getFunction("sea_dsa_set_modified");
  auto specFnRead = m_specModule->getFunction("sea_dsa_set_read");
  auto specFnCollapse = m_specModule->getFunction("sea_dsa_collapse");
  auto specFnHeap = m_specModule->getFunction("sea_dsa_set_heap");
  auto specFnP2I = m_specModule->getFunction("sea_dsa_set_ptrtoint");
  auto specFnI2P = m_specModule->getFunction("sea_dsa_set_inttoptr");
  auto specFnExtern = m_specModule->getFunction("sea_dsa_set_external");
  auto specFnAlias = m_specModule->getFunction("sea_dsa_alias");
  auto specFnMk = m_specModule->getFunction("sea_dsa_mk");

  if (F.getName() == "main") return;
  if (F.isDeclaration() || F.empty()) return;

  auto fnCallee =
      m_specModule->getOrInsertFunction(F.getName(), F.getFunctionType());

  llvm::Function *specFn = cast<llvm::Function>(fnCallee.getCallee());

  assert(&specFn->getContext() == &m_specModule->getContext() &&
         "Contexts different, unable to create calls");

  llvm::BasicBlock *block =
      llvm::BasicBlock::Create(F.getContext(), specFn->getName(), specFn);

  llvm::IRBuilder<> builder(block);

  auto getOrInsertAccessFnTy = [](const ModuleRef &specM,
                                  Type *ty) -> Function * {
    if (!ty) return specM->getFunction("sea_dsa_access");

    std::string tyStr;
    llvm::raw_string_ostream rso(tyStr);
    ty->print(rso);

    auto str = rso.str();
    Function *baseFn = specM->getFunction("sea_dsa_access");

    auto baseFnTy = baseFn->getFunctionType();
    llvm::FunctionType *derivedFnTy = llvm::FunctionType::get(
        baseFnTy->getReturnType(), {ty, baseFnTy->getParamType(1)}, false);

    return llvm::dyn_cast<Function, Value>(
        specM->getOrInsertFunction("sea_dsa_access_" + str, derivedFnTy)
            .getCallee());
  };

  auto getOrInsertLinkFnTy = [](const ModuleRef &specM,
                                Type *ty) -> Function * {
    if (!ty) return specM->getFunction("sea_dsa_link");

    std::string tyStr;
    llvm::raw_string_ostream rso(tyStr);
    ty->print(rso);

    auto str = rso.str();
    Function *baseFn = specM->getFunction("sea_dsa_link");

    auto baseFnTy = baseFn->getFunctionType();
    llvm::FunctionType *derivedFnTy = llvm::FunctionType::get(
        baseFnTy->getReturnType(),
        {baseFnTy->getParamType(0), baseFnTy->getParamType(1), ty}, false);

    return llvm::dyn_cast<Function, Value>(
        specM->getOrInsertFunction("sea_dsa_link_to" + str, derivedFnTy)
            .getCallee());
  };

  // sets the attributes that the original node has onto the spec graph value
  auto setAttributes = [&](const Node *gNode, Value *specVal) {
    auto bitCastVal = builder.CreateBitCast(specVal, builder.getInt8PtrTy());

    if (gNode->isModified()) { builder.CreateCall(specFnModify, bitCastVal); }
    if (gNode->isHeap()) { builder.CreateCall(specFnHeap, bitCastVal); }
    if (gNode->isRead()) { builder.CreateCall(specFnRead, bitCastVal); }
    if (gNode->isOffsetCollapsed()) {
      builder.CreateCall(specFnCollapse, bitCastVal);
    }
    if (gNode->isPtrToInt()) { builder.CreateCall(specFnP2I, bitCastVal); }
    if (gNode->isIntToPtr()) { builder.CreateCall(specFnI2P, bitCastVal); }
    if (gNode->isExternal()) { builder.CreateCall(specFnExtern, bitCastVal); }
  };

  std::stack<std::pair<Node *, Value *>> visitStack;
  llvm::DenseMap<Node *, Value *> aliasMap;

  auto fIt = F.arg_begin();
  auto specIt = specFn->arg_begin();

  for (; fIt != F.arg_end(); ++fIt, ++specIt) {
    if (!fIt->getType()->isPointerTy()) continue;
    if (!G->hasCell(*fIt)) continue;

    Value &v = *specIt;
    Value *castVal = builder.CreateBitCast(&v, builder.getInt8PtrTy());
    visitStack.push({G->getCell(*fIt).getNode(), castVal});
  }

  Value *retVal = nullptr;
  if (F.getReturnType()->isPointerTy() && G->hasRetCell(F)) {
    retVal = builder.CreateCall(specFnMk, llvm::None, "ret");
    visitStack.push({G->getRetCell(F).getNode(), retVal});
  }

  while (!visitStack.empty()) {
    auto graphNode = visitStack.top().first;
    auto specVal = visitStack.top().second;
    visitStack.pop();

    // we have already visited this node
    if (aliasMap.count(graphNode)) {
      builder.CreateCall(specFnAlias,
                         {aliasMap.find(graphNode)->second, specVal});
    } else {
      for (auto &link : graphNode->links()) {
        Value *newNodeVal = builder.CreateCall(specFnMk);

        Type *ty = link.first.getType().getLLVMType();
        unsigned offset = link.first.getOffset();

        auto uIntTy = llvm::Type::getInt32Ty(m_specModule->getContext());
        auto llvmOffset = llvm::ConstantInt::get(
            uIntTy, llvm::APInt(uIntTy->getIntegerBitWidth(), offset, false));

        auto linkFn = getOrInsertLinkFnTy(m_specModule, ty);
        Value *castChild;
        if (ty)
          castChild = builder.CreateBitCast(newNodeVal, ty);
        else
          castChild = builder.CreateBitCast(
              newNodeVal, llvm::Type::getInt8PtrTy(m_specModule->getContext()));
        builder.CreateCall(linkFn, {specVal, llvmOffset, castChild});

        visitStack.push({link.second->getNode(), newNodeVal});
      }

      // set all accessed types of the node
      for (auto &typeIt : graphNode->types()) {
        unsigned offset = typeIt.first;
        auto typeSet = typeIt.second;

        auto uIntTy = llvm::Type::getInt32Ty(m_specModule->getContext());
        auto llvmOffset = llvm::ConstantInt::get(
            uIntTy, llvm::APInt(uIntTy->getIntegerBitWidth(), offset, false));

        for (auto type : typeSet) {
          if (!type->isPointerTy()) continue;

          auto mutTy = const_cast<Type *>(type);
          Function *accessFn = getOrInsertAccessFnTy(m_specModule, mutTy);

          auto castVal = builder.CreateBitCast(specVal, mutTy);
          builder.CreateCall(accessFn, {castVal, llvmOffset});
        }
      }

      // set the attributes for the current node
      setAttributes(graphNode, specVal);
      // assert that we have visited this node
      aliasMap.insert({graphNode, specVal});
    }
  }

  // handle the return cell
  if (G->hasRetCell(F) && F.getReturnType()->isPointerTy()) {
    retVal = builder.CreateBitCast(retVal, F.getReturnType(), "castRet");
    builder.CreateRet(retVal);
  } else if (!retVal && F.getReturnType()->getTypeID()) {
    retVal = builder.CreateAlloca(F.getReturnType(), nullptr);
    Value *loadedRet = builder.CreateLoad(retVal->getType()->getPointerElementType(), retVal);
    builder.CreateRet(loadedRet);
  }
}

void DsaLibFuncInfo::writeSpecModule() const {
  std::error_code ec;
  llvm::raw_fd_ostream ofs(m_specModule->getSourceFileName(), ec);
  m_specModule->print(ofs, nullptr);
}

char DsaLibFuncInfo::ID = 0;
} // namespace seadsa

namespace seadsa {
llvm::Pass *createDsaLibFuncInfoPass() { return new DsaLibFuncInfo(); }
} // namespace seadsa

using namespace seadsa;
using namespace llvm;

INITIALIZE_PASS_BEGIN(DsaLibFuncInfo, "seadsa-spec-graph-info",
                      "Creates local analysis from spec", false, false)
INITIALIZE_PASS_END(DsaLibFuncInfo, "seadsa-spec-graph-info",
                    "Creates local analysis from spec", false, false)
