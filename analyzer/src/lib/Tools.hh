#ifndef TOOLS_H
#define TOOLS_H
#include <llvm/IR/BasicBlock.h>

#include "Analyzer.hh"

namespace Tools {
  using namespace llvm;
  bool from_bba_to_bbb(BasicBlock *a, BasicBlock *b);
  bool from_bba_to_bbc_via_bbb(BasicBlock *a, BasicBlock *b, BasicBlock *c);
  bool from_bba_to_bbc_not_via_bbbs(BasicBlock *a, BasicBlockSet *b,
                                    BasicBlock *c);

  void find_ret_bb(BasicBlockSet *via_bbs, BasicBlockSet *ret_bbs);
  void source_analyze(Function *func, ValueSet *involved_set);
  bool less_than_instructions(Instruction *ins1, Instruction *ins2);
}

#endif
