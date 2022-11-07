#ifndef EXPRESSIONGRAPH_H
#define EXPRESSIONGRAPH_H

#include "llvm/Pass.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "Analyzer.hh"
#include "assert.h"

#include "PEGNode.hh"
using namespace llvm;


class ExpressionGraphPass : public IterativeModulePass {
private:
  llvm::DenseMap<ConstantExpr *, Instruction *> constantexpr_ins_map;
  const llvm::DataLayout *datalayout;
  std::map<PEG::PEGNode *, llvm::SmallPtrSet<PEG::PEGFieldNode *, 8>>
  node_fields_map;

  void add_global_value(llvm::Module *M);
  Value *create_constantexpr(Value *v);
  Value *translate_constantexpr(Value *v,
                                PEG::PEGEdgeList *edge_list);
  PEG::PEGNode *get_pegnode(Value *v);
  void translate_function(Function *f);
  void translate_basicblock(BasicBlock *bb);
  bool create_instruction_node(Instruction *ins);
  bool map_instruction(Instruction *ins);
  bool translate_instruction(Instruction *ins,
                             PEG::PEGEdgeList *edge_list);
public:
  llvm::DenseMap<Value *, PEG::PEGNode *> value_pegnode_map;
  ExpressionGraphPass(GlobalContext *Ctx_)
    : IterativeModulePass(Ctx_, "ExpressionGraph(PEG)") {}
  virtual bool doInitialization(llvm::Module *);
  virtual bool doFinalization(llvm::Module *);
  virtual bool doModulePass(llvm::Module *);
};

#endif
