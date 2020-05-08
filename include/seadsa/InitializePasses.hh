#pragma once

#include "llvm/InitializePasses.h"

namespace llvm {
void initializeRemovePtrToIntPass(PassRegistry &);
void initializeAllocWrapInfoPass(PassRegistry &);
void initializeAllocSiteInfoPass(PassRegistry &);  
void initializeDsaAnalysisPass(PassRegistry &);
void initializeCompleteCallGraphPass(PassRegistry &);
}
