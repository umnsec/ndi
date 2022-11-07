#ifndef RELEASEIDENTIFICATION_H
#define RELEASEIDENTIFICATION_H

#include "Analyzer.hh"

class ReleaseIdentificationPass : public IterativeModulePass {
private:

public:
  std::map<StringRef, int> callee_times;
  ReleaseIdentificationPass(GlobalContext *Ctx_)
    : IterativeModulePass(Ctx_, "release function identification analyzer") {
  }
  virtual bool doInitialization(llvm::Module *);
  virtual bool doFinalization(llvm::Module *);
  virtual bool doModulePass(llvm::Module *);
  void output();
};

#endif
