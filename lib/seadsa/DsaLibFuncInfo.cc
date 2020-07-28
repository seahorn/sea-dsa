
#include "llvm/ADT/Twine.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Pass.h"
#include "llvm/PassRegistry.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/Registry.h"

#include "seadsa/DsaLibFuncInfo.hh"
#include "seadsa/Graph.hh"
#include "seadsa/InitializePasses.hh"
#include "seadsa/support/Debug.h"
#include <iostream>

static llvm::cl::list<std::string>
    XSpecFiles("sea-dsa-specfile", llvm::cl::desc("<Input spec bitcode file>"),
               llvm::cl::ZeroOrMore, llvm::cl::value_desc("filename"));

static llvm::cl::opt<bool> XUseClibSpec(
    "sea-dsa-use-clib-spec",
    llvm::cl::desc("Replace clib functions with spec defined by seadsa"),
    llvm::cl::Optional, llvm::cl::init(false));

namespace seadsa {

void DsaLibFuncInfo::getAnalysisUsage(llvm::AnalysisUsage &AU) const {
  AU.addRequired<seadsa::AllocWrapInfo>();
  AU.addRequired<llvm::TargetLibraryInfoWrapperPass>();
  AU.setPreservesAll();
}

void DsaLibFuncInfo::initialize(const llvm::Module &M) const {

  using namespace llvm::sys::fs;
  using namespace llvm;

  if (m_isInitialized || (XSpecFiles.empty() && !XUseClibSpec)) return;
  m_isInitialized = true;
  auto dl = M.getDataLayout();

  SMDiagnostic err;

  auto exePath = getMainExecutable(nullptr, nullptr);
  StringRef exeDir = llvm::sys::path::parent_path(exePath);

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
} // namespace seadsa

bool DsaLibFuncInfo::hasSpecFunc(const llvm::Function &F) const {
  return m_funcs.count(F.getName());
}

llvm::Function *DsaLibFuncInfo::getSpecFunc(const llvm::Function &F) const {
  auto it = m_funcs.find(F.getName());
  assert(it != m_funcs.end());
  return it->second;
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
