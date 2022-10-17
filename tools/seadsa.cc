///
// seadsda -- Print heap graphs and call graph computed by sea-dsa
///

#include "llvm/Analysis/CallPrinter.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/InitializePasses.h"
#include "llvm/LinkAllPasses.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO.h"

#include "seadsa/CompleteCallGraph.hh"
#include "seadsa/DsaAnalysis.hh"
#include "seadsa/DsaLibFuncInfo.hh"
#include "seadsa/InitializePasses.hh"
#include "seadsa/SeaDsaAliasAnalysis.hh"
#include "seadsa/ShadowMem.hh"
#include "seadsa/support/Debug.h"
#include "seadsa/support/RemovePtrToInt.hh"

static llvm::cl::opt<std::string>
    InputFilename(llvm::cl::Positional,
                  llvm::cl::desc("<input LLVM bitcode file>"),
                  llvm::cl::Required, llvm::cl::value_desc("filename"));

static llvm::cl::opt<std::string>
    OutputDir("outdir", llvm::cl::desc("Output directory for oll"),
              llvm::cl::init(""), llvm::cl::value_desc("DIR"));

static llvm::cl::opt<std::string>
    AsmOutputFilename("oll", llvm::cl::desc("Output analyzed bitcode"),
                      llvm::cl::init(""), llvm::cl::value_desc("filename"));

static llvm::cl::opt<std::string> DefaultDataLayout(
    "data-layout",
    llvm::cl::desc("data layout string to use if not specified by module"),
    llvm::cl::init(""), llvm::cl::value_desc("layout-string"));

static llvm::cl::opt<bool> MemDot(
    "sea-dsa-dot",
    llvm::cl::desc("Print SeaDsa memory graph of each function to dot format"),
    llvm::cl::init(false));

static llvm::cl::opt<bool> MemViewer(
    "sea-dsa-viewer",
    llvm::cl::desc("View SeaDsa memory graph of each function to dot format"),
    llvm::cl::init(false));

static llvm::cl::opt<bool> CallGraphDot(
    "sea-dsa-callgraph-dot",
    llvm::cl::desc("Print SeaDsa complete call graph to dot format"),
    llvm::cl::init(false));

static llvm::cl::opt<bool> RunShadowMem("sea-dsa-shadow-mem",
                                        llvm::cl::desc("Run ShadowMemPass"),
                                        llvm::cl::Hidden,
                                        llvm::cl::init(false));
static llvm::cl::opt<bool>
    AAEval("sea-dsa-aa-eval",
           llvm::cl::desc(
               "Exhaustive Alias Analaysis Precision Evaluation using seadsa"),
           llvm::cl::init(false));

namespace seadsa {
SeaDsaLogOpt loc;
extern bool PrintDsaStats;
extern bool PrintCallGraphStats;
} // namespace seadsa

static llvm::cl::opt<seadsa::SeaDsaLogOpt, true, llvm::cl::parser<std::string>>
    LogClOption("log", llvm::cl::desc("Enable specified log level"),
                llvm::cl::location(seadsa::loc), llvm::cl::value_desc("string"),
                llvm::cl::ValueRequired, llvm::cl::ZeroOrMore);

/// Changes the \p path to be inside specified \p outdir
///
/// Creates output directory if necessary.
/// Returns input \p path if directory cannot be changed
static std::string withDir(const std::string &outdir, const std::string &path) {
  if (!outdir.empty()) {
    if (!llvm::sys::fs::create_directories(outdir)) {
      auto fname = llvm::sys::path::filename(path);
      return outdir + "/" + fname.str();
    }
  }
  return path;
}

int main(int argc, char **argv) {
  llvm::llvm_shutdown_obj shutdown; // calls llvm_shutdown() on exit
  llvm::cl::ParseCommandLineOptions(argc, argv, "Heap Analysis");

  llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);
  llvm::PrettyStackTraceProgram PSTP(argc, argv);
  llvm::EnableDebugBuffering = true;

  llvm::SMDiagnostic err;
  llvm::LLVMContext context;
  std::unique_ptr<llvm::Module> module;
  std::unique_ptr<llvm::ToolOutputFile> asmOutput;

  module = llvm::parseIRFile(InputFilename, err, context);
  if (module.get() == 0) {
    if (llvm::errs().has_colors())
      llvm::errs().changeColor(llvm::raw_ostream::RED);
    llvm::errs() << "error: "
                 << "Bitcode was not properly read; " << err.getMessage()
                 << "\n";
    if (llvm::errs().has_colors()) llvm::errs().resetColor();
    return 3;
  }

  if (!AsmOutputFilename.empty()) {
    std::error_code error_code;
    asmOutput = std::make_unique<llvm::ToolOutputFile>(
        withDir(OutputDir, AsmOutputFilename), error_code,
        llvm::sys::fs::OF_Text);
    if (error_code) {
      if (llvm::errs().has_colors())
        llvm::errs().changeColor(llvm::raw_ostream::RED);
      llvm::errs() << "error: Could not open " << AsmOutputFilename << ": "
                   << error_code.message() << "\n";
      if (llvm::errs().has_colors()) llvm::errs().resetColor();
      return 3;
    }
  }

  ///////////////////////////////
  // initialise and run passes //
  ///////////////////////////////

  llvm::legacy::PassManager pass_manager;

  llvm::PassRegistry &Registry = *llvm::PassRegistry::getPassRegistry();
  llvm::initializeAnalysis(Registry);

  /// call graph and other IPA passes
  // llvm::initializeIPA (Registry);
  // XXX: porting to 3.8
  llvm::initializeCallGraphWrapperPassPass(Registry);
  // XXX: porting to 5.0
  //  llvm::initializeCallGraphPrinterPass(Registry);
  llvm::initializeCallGraphViewerPass(Registry);
  // XXX: not sure if needed anymore
  // llvm::initializeGlobalsAAWrapperPassPass(Registry);

  llvm::initializeRemovePtrToIntPass(Registry);
  llvm::initializeDsaAnalysisPass(Registry);
  llvm::initializeAllocWrapInfoPass(Registry);
  llvm::initializeDsaLibFuncInfoPass(Registry);
  llvm::initializeAllocSiteInfoPass(Registry);
  llvm::initializeCompleteCallGraphPass(Registry);

  llvm::initializeSeaDsaAAWrapperPassPass(Registry);

  // add an appropriate DataLayout instance for the module
  const llvm::DataLayout *dl = &module->getDataLayout();
  if (!dl && !DefaultDataLayout.empty()) {
    module->setDataLayout(DefaultDataLayout);
    dl = &module->getDataLayout();
  }

  assert(dl && "Could not find Data Layout for the module");

  pass_manager.add(seadsa::createRemovePtrToIntPass());
  // ==--== Alias Analysis Passes ==--==/

  // -- add to pass manager
  pass_manager.add(seadsa::createDsaLibFuncInfoPass());
  pass_manager.add(seadsa::createSeaDsaAAWrapperPass());
  // -- make available through AAResultsWrapperPass via ExternalAAWrapperPass
  pass_manager.add(llvm::createExternalAAWrapperPass(
      [](llvm::Pass &P, llvm::Function &, llvm::AAResults &AAR) {
        if (auto *WrapperPass =
                P.getAnalysisIfAvailable<seadsa::SeaDsaAAWrapperPass>())
          AAR.addAAResult(WrapperPass->getResult());
      }));

  // XXX Comment other alias analyses for now to make sure that we
  // XXX get to SeaDsa one first. Enable them once there are
  // XXX tests that exercise alias analysis
  // pass_manager.add(llvm::createTypeBasedAAWrapperPass());
  // pass_manager.add(llvm::createScopedNoAliasAAWrapperPass());

  // compute mod-ref information about functions . Good idea to (re)run it after
  // indirect calls have been resolved See comments in PassManagerBuilder.cpp on
  // how it interacts with Function passes
  // pass_manager.add(llvm::createGlobalsAAWrapperPass());
  // ==--== End of Alias Analysis Passes ==--==/

  if (RunShadowMem) {
    pass_manager.add(seadsa::createShadowMemPass());
  } else {
    if (MemDot) { pass_manager.add(seadsa::createDsaPrinterPass()); }

    if (MemViewer) { pass_manager.add(seadsa::createDsaViewerPass()); }

    if (seadsa::PrintDsaStats && !MemDot && !MemViewer) {
      pass_manager.add(seadsa::createDsaPrintStatsPass());
    }

    if (seadsa::PrintCallGraphStats) {
      pass_manager.add(seadsa::createDsaPrintCallGraphStatsPass());
    }

    if (CallGraphDot) {
      pass_manager.add(seadsa::createDsaCallGraphPrinterPass());
    }

    if (AAEval) { pass_manager.add(llvm::createAAEvalPass()); }

    if (!MemDot && !MemViewer && !seadsa::PrintDsaStats &&
        !seadsa::PrintCallGraphStats && !CallGraphDot && !AAEval) {
      llvm::errs() << "No option selected: choose one option between "
                   << "{sea-dsa-dot, sea-dsa-viewer, sea-dsa-stats, "
                   << "sea-dsa-callgraph-dot, sea-dsa-callgraph-stats, "
                      "sea-dsa-aa-eval}\n";
    }
  }

  if (!AsmOutputFilename.empty())
    pass_manager.add(createPrintModulePass(asmOutput->os()));

  pass_manager.run(*module.get());

  if (!AsmOutputFilename.empty()) asmOutput->keep();

  return 0;
}
