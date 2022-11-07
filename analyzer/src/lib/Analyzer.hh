#ifndef ANALYZER_GLOBAL_H
#define ANALYZER_GLOBAL_H

#include <llvm/IR/DebugInfo.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/ADT/StringExtras.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Analysis/AliasAnalysis.h>
#include "llvm/Support/CommandLine.h"
#include <map>
#include <unordered_map>
#include <set>
#include <list>
#include <unordered_set>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

#include "Common.hh"
#include "RIDCommon.hh"

#include "PEGNode.hh"
//
// typedefs
//
typedef vector< pair<llvm::Module *, llvm::StringRef> > ModuleList;
// Mapping module to its file name.
typedef unordered_map<llvm::Module *, llvm::StringRef> ModuleNameMap;
// The set of all functions.
typedef llvm::SmallPtrSet<llvm::Function *, 8> FuncSet;
// Mapping from function name to function.
typedef unordered_map<string, llvm::Function *> NameFuncMap;
typedef llvm::SmallPtrSet<llvm::CallInst *, 8> CallInstSet;
typedef std::map<Function *, CallInstSet> CallerMap;
typedef std::map<CallInst *, FuncSet> CalleeMap;

// Pointer analysis types.
typedef DenseMap<Value *, SmallPtrSet<Value *, 16>> PointerAnalysisMap;
typedef unordered_map<Function *, PointerAnalysisMap> FuncPointerAnalysisMap;
typedef unordered_map<Function *, AAResults *> FuncAAResultsMap;

typedef llvm::SmallPtrSet<Value *, 8> ValueSet;
typedef llvm::SmallPtrSet<BasicBlock *, 8> BasicBlockSet;
typedef std::list<Instruction *> InstructionList;
typedef llvm::SmallPtrSet<Instruction *, 8> InstructionSet;

struct GlobalContext {
  GlobalContext() {
    // Initialize statistucs.
  }

  // Map global function name to function.
  NameFuncMap GlobalFuncs;

  // Functions whose addresses are taken.
  FuncSet AddressTakenFuncs;

  // Map a callsite to all potential callee functions.
  CalleeMap Callees;

  // Map a function to all potential caller instructions.
  CallerMap Callers;

  // Indirect call instructions.
  std::vector<CallInst *>IndirectCallInsts;

  // Unified functions -- no redundant inline functions
  DenseMap<size_t, Function *>UnifiedFuncMap;
  set<Function *>UnifiedFuncSet;

  // Map function signature to functions
  DenseMap<size_t, FuncSet>sigFuncsMap;

  // Modules.
  ModuleList Modules;
  ModuleNameMap ModuleMaps;
  set<string> InvolvedModules;

  // SecurityChecksPass
  // Functions handling errors
  set<string> ErrorHandleFuncs;
  map<string, tuple<int8_t, int8_t, int8_t>> CopyFuncs;

  // Pointer analysis results.
  FuncPointerAnalysisMap FuncPAResults;
  FuncAAResultsMap FuncAAResults;

  // start rid check
  // save function-path map
  FunctionPathsMap plain_func_map;
  FuncSet callee_set;
  FuncSet kfree_fun_set;
  FuncSet refcnt_fun_set;
  FuncSet retval_fun_set;
  FuncSet init_fun_set;
  set<string> release_funcs;
  //std::map<std::string, std::set<std::string>> module_dependency;

  std::map<BasicBlock *, PEG::PEGEdgeList> bb_edges_map;
  llvm::DenseMap<Argument *, PEG::PEGNode *> arg_node_map;
  llvm::DenseMap<Function *, PEG::PEGNode *> func_retnode_map;

  llvm::DenseMap<PEG::PEGCallNode *, FuncSet *> callnode_funcs_map;
  llvm::DenseMap<Function *, EdgeInformation> func_info_map;

  std::map<Function *, TargetFunctionState> func_pairs_map;
};

class IterativeModulePass {
protected:
  GlobalContext *Ctx;
  const char *ID;
public:
  IterativeModulePass(GlobalContext *Ctx_, const char *ID_)
    : Ctx(Ctx_), ID(ID_) { }

  // Run on each module before iterative pass.
  virtual bool doInitialization(__attribute__((unused)) llvm::Module *M)
  { return true; }

  // Run on each module after iterative pass.
  virtual bool doFinalization(__attribute__((unused)) llvm::Module *M)
  { return true; }

  // Iterative pass.
  virtual bool doModulePass(__attribute__((unused)) llvm::Module *M)
  { return false; }

  virtual void run(ModuleList &modules);
};

#endif
