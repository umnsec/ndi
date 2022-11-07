#ifndef FUNCTIONPATH_H
#define FUNCTIONPATH_H

#include "llvm/Pass.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "Analyzer.hh"

#include "RIDCommon.hh"
#define PATHLIMIT 200
using namespace llvm;

class FunctionPathsPass : public IterativeModulePass {
private:
  int total;
  int red;
  void dfs_on_function(BasicBlock *, FunctionPath *, FunctionPaths *, int &);
  FunctionPaths *make_function_paths(Function *);

public:
  FunctionPathsPass(GlobalContext *Ctx_)
    : IterativeModulePass(Ctx_, "FunctionPathsGraph") {
    total = 0;
    red = 0;
  }
  virtual bool doInitialization(llvm::Module *);
  virtual bool doFinalization(llvm::Module *);
  virtual bool doModulePass(llvm::Module *);
};

#endif
