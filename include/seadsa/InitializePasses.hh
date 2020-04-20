#pragma once

#include "llvm/InitializePasses.h"

namespace llvm {
void initializeRemovePtrToIntPass(PassRegistry &);
void initializeDsaAnalysisPass(PassRegistry &);
}
