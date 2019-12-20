///
// seadsda -- Print heap graphs and call graph computed by sea-dsa
///

#include "llvm/LinkAllPasses.h"
#include "llvm/Analysis/CallPrinter.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/IR/Verifier.h"

#include "sea_dsa/DsaAnalysis.hh"
#include "sea_dsa/CompleteCallGraph.hh"
#include "sea_dsa/ShadowMem.hh"

static llvm::cl::opt<std::string>
InputFilename(llvm::cl::Positional, llvm::cl::desc("<input LLVM bitcode file>"),
              llvm::cl::Required, llvm::cl::value_desc("filename"));

static llvm::cl::opt<std::string>
OutputDir("outdir",
	  llvm::cl::desc("Output directory for oll"),
	  llvm::cl::init(""), llvm::cl::value_desc("DIR"));

static llvm::cl::opt<std::string>
AsmOutputFilename("oll", llvm::cl::desc("Output analyzed bitcode"),
               llvm::cl::init(""), llvm::cl::value_desc("filename"));

static llvm::cl::opt<std::string>
DefaultDataLayout("data-layout",
        llvm::cl::desc("data layout string to use if not specified by module"),
        llvm::cl::init(""), llvm::cl::value_desc("layout-string"));

static llvm::cl::opt<bool>
MemDot("sea-dsa-dot",
       llvm::cl::desc("Print SeaDsa memory graph of each function to dot format"),
       llvm::cl::init(false));

static llvm::cl::opt<bool>
MemViewer("sea-dsa-viewer",
	  llvm::cl::desc("View SeaDsa memory graph of each function to dot format"),
	  llvm::cl::init(false));

static llvm::cl::opt<bool>
CallGraphDot("sea-dsa-callgraph-dot",
	     llvm::cl::desc("Print SeaDsa complete call graph to dot format"),
	     llvm::cl::init(false));

static llvm::cl::opt<bool>
   RunShadowMem("sea-dsa-shadow-mem",
	  llvm::cl::desc("Run ShadowMemPass"),
	  llvm::cl::Hidden,
	  llvm::cl::init(false));

namespace sea_dsa {
  extern bool PrintDsaStats;
  extern bool PrintCallGraphStats;
}

static std::string appendOutDir(std::string path) {
  if (!OutputDir.empty()) {
    auto filename = llvm::sys::path::filename(path);
    if (!llvm::sys::fs::create_directory(OutputDir)) {
      std::string FullFileName = OutputDir + "/" + filename.str();
      return FullFileName;
    }
  }
  return path;
}

int main(int argc, char **argv) {
  llvm::llvm_shutdown_obj shutdown;  // calls llvm_shutdown() on exit
  llvm::cl::ParseCommandLineOptions(argc, argv, "Heap Analysis");

  llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);
  llvm::PrettyStackTraceProgram PSTP(argc, argv);
  llvm::EnableDebugBuffering = true;

  std::error_code error_code;
  llvm::SMDiagnostic err;
  llvm::LLVMContext context;
  std::unique_ptr<llvm::Module> module;
  std::unique_ptr<llvm::tool_output_file> asmOutput, output;

  module = llvm::parseIRFile(InputFilename, err, context);
  if (module.get() == 0)
  {
    if (llvm::errs().has_colors()) llvm::errs().changeColor(llvm::raw_ostream::RED);
    llvm::errs() << "error: "
                 << "Bitcode was not properly read; " << err.getMessage() << "\n";
    if (llvm::errs().has_colors()) llvm::errs().resetColor();
    return 3;
  }

  if (!AsmOutputFilename.empty ()) {
    asmOutput = llvm::make_unique<llvm::tool_output_file>
      (appendOutDir(AsmOutputFilename.c_str()), 
       error_code, llvm::sys::fs::F_Text);
       
  }
  if (error_code) {
    if (llvm::errs().has_colors())
      llvm::errs().changeColor(llvm::raw_ostream::RED);
    llvm::errs() << "error: Could not open " << AsmOutputFilename << ": "
                 << error_code.message () << "\n";
    if (llvm::errs().has_colors()) llvm::errs().resetColor();
    return 3;
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
  llvm::initializeGlobalsAAWrapperPassPass(Registry);  

  // add an appropriate DataLayout instance for the module
  const llvm::DataLayout *dl = &module->getDataLayout ();
  if (!dl && !DefaultDataLayout.empty ()) {
    module->setDataLayout (DefaultDataLayout);
    dl = &module->getDataLayout ();
  }

  assert (dl && "Could not find Data Layout for the module");  

  if (RunShadowMem) {
    pass_manager.add(sea_dsa::createShadowMemPass());
  } else {
    if (MemDot) {
      pass_manager.add(sea_dsa::createDsaPrinterPass());
    }

    if (MemViewer) {
      pass_manager.add(sea_dsa::createDsaViewerPass());
    }

    if (sea_dsa::PrintDsaStats) {
      pass_manager.add(sea_dsa::createDsaPrintStatsPass());
    }

    if (sea_dsa::PrintCallGraphStats) {
      pass_manager.add(sea_dsa::createDsaPrintCallGraphStatsPass());
    }

    if (CallGraphDot) {
      pass_manager.add(sea_dsa::createDsaCallGraphPrinterPass());
    }

    if (!MemDot && !MemViewer && !sea_dsa::PrintDsaStats &&
	!sea_dsa::PrintCallGraphStats && !CallGraphDot) {
      llvm::errs() << "No option selected: choose one option between "
		   << "{sea-dsa-dot, sea-dsa-viewer, sea-dsa-stats, "
		 << "sea-dsa-callgraph-dot, sea-dsa-callgraph-stats}\n";
    }
  }
  
  if (!AsmOutputFilename.empty ())
    pass_manager.add (createPrintModulePass (asmOutput->os ()));
  
  pass_manager.run(*module.get());

  if (!AsmOutputFilename.empty ())
    asmOutput->keep ();


  
  return 0;
}
