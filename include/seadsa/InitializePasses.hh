#pragma once

#include "llvm/InitializePasses.h"

namespace llvm {
void initializeRemovePtrToIntPass(PassRegistry &);
void initializeAllocWrapInfoPass(PassRegistry &);
void initializeAllocSiteInfoPass(PassRegistry &);
void initializeDsaLibFuncInfoPass(PassRegistry &);
void initializeDsaAnalysisPass(PassRegistry &);
void initializeCompleteCallGraphPass(PassRegistry &);
void initializeDsaInfoPassPass(PassRegistry &);
void initializeShadowMemPassPass(PassRegistry &);
void initializeStripShadowMemPassPass(PassRegistry &);
void initializeSeaDsaAAWrapperPassPass(PassRegistry &);
} // namespace llvm
