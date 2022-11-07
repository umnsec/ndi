#ifndef PEGZ3_H
#define PEGZ3_H

#include "z3++.h"
#include "PEGNode.hh"
#include "RIDCommon.hh"

namespace PEGZ3{

  extern z3::context c;
  extern z3::solver *s;
  void init_z3();

  extern llvm::DenseMap<PEG::PEGNode *, z3::expr *> pegnode_expr_map;
  z3::expr* get_andersen_expr_map(PEG::PEGNode *);
  bool add_expr(PEG::PEGNode *lhs,
                llvm::CmpInst::Predicate op,
                PEG::PEGNode *rhs);
  bool add_functionpath_constraints(FunctionPath *fp);
  bool add_functionpath_pegnode(FunctionPath *fp,
                                PEG::PEGNode *node);
  void clear_solver();
  bool check_solver();
  bool constraints_judgement(FunctionPath *fp);
  bool constraints_judgement_without_z3(FunctionPath *fp);
}

#endif
