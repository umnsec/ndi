#ifndef VALUEFLOWPASS_H
#define VALUEFLOWPASS_H
#include "Analyzer.hh"
#include "PEGNode.hh"

class ValueFlowPass : public IterativeModulePass {
private:
  void set_alias_value(FunctionPath *fp,
                       PEG::PEGNode *node, PEG::SmallPEGNodeSet *location);
  bool translate_edge(FunctionPath *fp, BasicBlock *curr, PEG::PEGEdge *edge);
  void translate_functionpath(FunctionPath *fp);
  void collect_constraints(FunctionPath *fp, BasicBlock *curr,
                           PEG::PEGEdge *cond_edge);
public:
  ValueFlowPass(GlobalContext *Ctx_)
    : IterativeModulePass(Ctx_, "Value flow contruction") {}
  virtual bool doInitialization(llvm::Module *);
  virtual bool doFinalization(llvm::Module *);
  virtual bool doModulePass(llvm::Module *);
};

#endif
