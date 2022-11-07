//===-- Analyzer.cc - the kernel-analysis framework--------------===//
//
// This file implements the analysis framework. It calls the pass for
// building call-graph and the pass for finding security checks.
//
// ===-----------------------------------------------------------===//
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/Support/SystemUtils.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/IR/InstIterator.h"

#include <memory>
#include <vector>
#include <sstream>
#include <sys/resource.h>

#include "Analyzer.hh"
#include "Config.hh"

#include "PEGZ3.hh"
#include "PEGNode.hh"
#include "CallGraph.hh"
#include "FunctionPathsPass.hh"
#include "ExpressionGraph.hh"
#include "ValueFlowPass.hh"
#include "CriticalVariable.hh"
#include "ReleaseIdentification.hh"

using namespace llvm;

// Command line parameters.
cl::list<string> InputFilenames(
    cl::Positional, cl::OneOrMore, cl::desc("<input bitcode files>"));

cl::opt<unsigned> VerboseLevel(
    "verbose-level", cl::desc("Print information at which verbose level"),
    cl::init(0));

cl::opt<bool> ReleaseCollection(
    "release",
    cl::desc("collect release function"),
    cl::NotHidden, cl::init(false));

GlobalContext GlobalCtx;


void IterativeModulePass::run(ModuleList &modules) {

  ModuleList::iterator i, e;
  OP << "[" << ID << "] Initializing " << modules.size() << " modules ";
  bool again = true;
  while (again) {
    again = false;
    for (i = modules.begin(), e = modules.end(); i != e; ++i) {
      again |= doInitialization(i->first);
      OP << ".";
    }
  }
  OP << "\n";

  unsigned iter = 0, changed = 1;
  while (changed) {
    ++iter;
    changed = 0;
    unsigned counter_modules = 0;
    unsigned total_modules = modules.size();
    for (i = modules.begin(), e = modules.end(); i != e; ++i) {
      OP << "[" << ID << " / " << iter << "] ";
      OP << "[" << ++counter_modules << " / " << total_modules << "] ";
      OP << "[" << i->second << "]\n";

      bool ret = doModulePass(i->first);
      if (ret) {
        ++changed;
        OP << "\t [CHANGED]\n";
      } else
        OP << "\n";
    }
    OP << "[" << ID << "] Updated in " << changed << " modules.\n";
  }

  OP << "[" << ID << "] Postprocessing ...\n";
  again = true;
  while (again) {
    again = false;
    for (i = modules.begin(), e = modules.end(); i != e; ++i) {
      // TODO: Dump the results.
      again |= doFinalization(i->first);
    }
  }

  OP << "[" << ID << "] Done!\n\n";
}

void LoadStaticData(GlobalContext *GCtx) {
  // Load relase functions -- may be put in the very beginning
  SetReleaseFuncs(GlobalCtx.release_funcs);
  // Load error-handling functions
  SetErrorHandleFuncs(GCtx->ErrorHandleFuncs);
  // load functions that copy/move values
  SetCopyFuncs(GCtx->CopyFuncs);
  //SetModuleDependency(GCtx->module_dependency);
}

//void ProcessResults(GlobalContext *GCtx) {}

int main(int argc, char **argv) {
  // Print a stack trace if we signal out.
  sys::PrintStackTraceOnErrorSignal(argv[0]);
  PrettyStackTraceProgram X(argc, argv);
  llvm_shutdown_obj Y;  // Call llvm_shutdown() on exit.
  cl::ParseCommandLineOptions(argc, argv, "global analysis\n");
  SMDiagnostic Err;
  //SetReleaseFuncs(GlobalCtx.release_funcs);
  // handle InputFiles[0]: dictionary or file
  if (llvm::sys::fs::is_directory(InputFilenames[0])) {
    std::error_code ec;
    llvm::sys::fs::recursive_directory_iterator it(InputFilenames[0], ec);
    llvm::sys::fs::recursive_directory_iterator end_it;
    //llvm::sys::fs::directory_iterator it(InputFilenames[0], ec);
    //llvm::sys::fs::directory_iterator end_it;
    InputFilenames.clear();
    string ending = ".bc";
    int len = ending.size();
    for (; it != end_it; it.increment(ec)) {
      if (!it->path().compare(it->path().size() - len, len, ending))
        InputFilenames.push_back(it->path());
    }
  }

  // Main workflow
  LoadStaticData(&GlobalCtx);

  // Loading modules
  OP << "Total " << InputFilenames.size() << " file(s)\n";
  // i will be 1 in former testing
  for (unsigned i = 0; i < InputFilenames.size(); ++i) {
    LLVMContext *LLVMCtx = new LLVMContext();
    OP << InputFilenames[i] << "\n";
    unique_ptr<Module> M = parseIRFile(InputFilenames[i], Err, *LLVMCtx);

    if (M == NULL) {
      OP << argv[0] << ": error loading file '"
         << InputFilenames[i] << "'\n";
      continue;
    }
    Module *Module = M.release();
    StringRef MName = StringRef(strdup(InputFilenames[i].data()));
    GlobalCtx.Modules.push_back(make_pair(Module, MName));
    GlobalCtx.ModuleMaps[Module] = InputFilenames[i];
  }

  if (ReleaseCollection) {
    ReleaseIdentificationPass ri_pass(&GlobalCtx);
    ri_pass.run(GlobalCtx.Modules);
    ri_pass.output();
    return 0;
  }
  PEG::init_peg_env();
  PEGZ3::init_z3();
  // Build global callgraph.
  CallGraphPass CGPass(&GlobalCtx);
  CGPass.run(GlobalCtx.Modules);
  // we should RIDInit function
  // Expression graph pass: translate IR into graph expression.
  ExpressionGraphPass peg(&GlobalCtx);
  peg.run(GlobalCtx.Modules);
  // Function path pass: collect function path.
  FunctionPathsPass path_pass(&GlobalCtx);
  path_pass.run(GlobalCtx.Modules);
  //InterCFGBuilderPass cfg_builder(&GlobalCtx);
  //cfg_builder.run(GlobalCtx.Modules);
  // Value flow pass: perform dataflow analysis on a certain path
  ValueFlowPass vfpass(&GlobalCtx);
  vfpass.run(GlobalCtx.Modules);
  // Critical vriable pass:
  // 1. identify critical variable.
  // 2. collect dist.
  // 3. perform 2-staged use-recovery analysis
  CriticalVariablePass cvpass(&GlobalCtx);
  cvpass.run(GlobalCtx.Modules);
  return 0;
}

