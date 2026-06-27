#pragma once
/// Per-function TargetLibraryInfo accessor used throughout sea-dsa.
///
/// sea-dsa originally threaded a legacy llvm::TargetLibraryInfoWrapperPass
/// through its analyses purely to call wrapper.getTLI(F). That ties sea-dsa to
/// the legacy pass object. Instead the analyses now take this getter: callers
/// supply it backed by whatever TLI source they have -- a legacy wrapper, or
/// the new-PM TargetLibraryAnalysis obtained from a FunctionAnalysisManager.
#include <functional>

namespace llvm {
class Function;
class TargetLibraryInfo;
class TargetLibraryInfoWrapperPass;
} // namespace llvm

namespace seadsa {
using TargetLibraryInfoGetter =
    std::function<const llvm::TargetLibraryInfo &(const llvm::Function &)>;

/// Adapt a legacy TargetLibraryInfoWrapperPass into a getter (the wrapper must
/// outlive the returned getter).
TargetLibraryInfoGetter mkTLIGetter(llvm::TargetLibraryInfoWrapperPass &W);
} // namespace seadsa
