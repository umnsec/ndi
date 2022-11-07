#ifndef CRITICALVARIABLE_H
#define CRITICALVARIABLE_H

// change this macro to detect different inconsistenies.
// check it one by one, since they have different definations of uses
#define CV_RELEASE 0
#define CV_RET 1
#define CV_INIT 0

#include "Analyzer.hh"
#include "RIDCommon.hh"
#include "Distinguisher.hh"
#include "PEGFunctionBuilder.hh"

class CriticalVariablePass : public IterativeModulePass {
private:
  std::map<Function *, unsigned> callee_count;
  bool fp_ret_null_check(FunctionPath *fp);
  bool fp_ret_nonnull_check(FunctionPath *fp);
  bool constitute_pathpair(FunctionPath *, FunctionPath *);
  void collect_release(FunctionPath *,
                       FunctionPath *,
                       PEG::SmallPEGNodeSet *);
  void collect_ret(FunctionPath *,
                   FunctionPath *,
                   PEG::SmallPEGNodeSet *);
  void collect_init(FunctionPath *,
                    FunctionPath *,
                    PEG::SmallPEGNodeSet *);
  void checking_process(TargetFunctionState *tfs,
                        Function *f,
                        bool add);
  void collect_alias(PEG::SmallPEGNodeSet *cv_set,
                     FunctionPath *fp,
                     TargetFunctionState *tfs);
public:
  CriticalVariablePass(GlobalContext *Ctx_)
    : IterativeModulePass(Ctx_, "critical variable analyzer") {}
  virtual bool doInitialization(llvm::Module *);
  virtual bool doFinalization(llvm::Module *);
  virtual bool doModulePass(llvm::Module *);
};

#endif
