#include "PEGZ3.hh"

namespace PEGZ3 {
  z3::context c;
  z3::solver *s;

  void init_z3() {
    s = new z3::solver(c);
  }

  llvm::DenseMap<PEG::PEGNode *, z3::expr *> pegnode_expr_map;

  z3::expr* get_pegnode_expr_map(PEG::PEGNode *node) {
    if (!pegnode_expr_map.count(node)) {
      z3::expr *exp = nullptr;
      switch (node->type) {
      case (PEG::PEGNODE_TYPE::INT): {
        PEG::PEGIntNode *int_node = static_cast<PEG::PEGIntNode *>(node);
        int num = int_node->int_val;
        exp = new z3::expr(c.int_val(num));
        break;
      }
      case (PEG::PEGNODE_TYPE::STATIC): {
        if (node->index == 0)
          exp = new z3::expr(c.int_val(0));
        break;
      }
      case (PEG::PEGNODE_TYPE::POINTER):
      case (PEG::PEGNODE_TYPE::OBJECT):
      case (PEG::PEGNODE_TYPE::FIELD):
      case (PEG::PEGNODE_TYPE::SELECT):
      case (PEG::PEGNODE_TYPE::CALL): {
        string s = "pegnode_" + std::to_string(node->index);
        exp = new z3::expr(c.int_const(s.c_str()));
        break;
      }
      case (PEG::PEGNODE_TYPE::PHI):
      case (PEG::PEGNODE_TYPE::COND):
      case (PEG::PEGNODE_TYPE::RET):
        OP << *node << KRED << " shoud not come up in z3\n" << KNRM;
        break;
      case (PEG::PEGNODE_TYPE::CMP):
        break;
      }
      pegnode_expr_map[node] = exp;
    }
    return pegnode_expr_map[node];
  }

  bool add_expr(PEG::PEGNode *lhs,
                llvm::CmpInst::Predicate op,
                PEG::PEGNode *rhs) {
    z3::expr* lexpr = get_pegnode_expr_map(lhs);
    z3::expr* rexpr = get_pegnode_expr_map(rhs);
    if (lexpr == NULL || rexpr == NULL)
      return false;
    z3::expr cons_expr(c);
    switch (op) {
    case llvm::CmpInst::ICMP_NE:
      cons_expr = *lexpr != *rexpr;
      break;
    case llvm::CmpInst::ICMP_EQ:
      cons_expr = *lexpr == *rexpr;
      break;
    case llvm::CmpInst::ICMP_UGT:
    case llvm::CmpInst::ICMP_SGT:
      cons_expr = *lexpr > *rexpr;
      break;
    case llvm::CmpInst::ICMP_UGE:
    case llvm::CmpInst::ICMP_SGE:
      cons_expr = *lexpr >= *rexpr;
      break;
    case llvm::CmpInst::ICMP_ULT:
    case llvm::CmpInst::ICMP_SLT:
      cons_expr = *lexpr < *rexpr;
      break;
    case llvm::CmpInst::ICMP_ULE:
    case llvm::CmpInst::ICMP_SLE:
      cons_expr = *lexpr <= *rexpr;
      break;
    default:
      OP << KRED << "unhandled in z3 " << op << "\n" << KNRM;
      return false;
    }
    s->add(cons_expr);
    return true;
  }

  bool add_functionpath_constraints(FunctionPath *fp) {
    for (auto &cons_map : fp->constraints) {
      for (auto &cons : cons_map.second) {
         for (PEG::PEGNode *lhs : cons.lhs_node) {
           for (PEG::PEGNode *rhs : cons.rhs_node) {
             if (!add_expr(lhs, cons.op, rhs)) {
               OP << KRED
                  << "ERROR in \n" << KNRM
                  << *lhs << "\n" << *rhs << "\n";
             }
           }
         }
       }
    }
    return true;
  }

  bool add_functionpath_pegnode(FunctionPath *fp,
                                PEG::PEGNode *node) {
    if (node->type == PEG::PEGNODE_TYPE::STATIC ||
        node->type == PEG::PEGNODE_TYPE::INT)
      return false;
    for (auto &map_cons : fp->constraints) {
      for (auto &cons : map_cons.second) {
        for (auto lhs : cons.lhs_node) {
          if (lhs == node) {
            for (auto rhs : cons.rhs_node) {
              add_expr(lhs, cons.op, rhs);
            }
          }
        }
        for (auto rhs : cons.rhs_node) {
          if (rhs == node) {
            for (auto lhs : cons.lhs_node) {
              add_expr(lhs, cons.op, rhs);
            }
          }
        }
      }
    }
    return true;
  }

  void clear_solver() {
    s->reset();
  }

  bool check_solver() {
    if (s->check() == z3::sat)
      return true;
    return false;
  }

  bool constraints_judgement(FunctionPath *fp) {
    clear_solver();
    add_functionpath_constraints(fp);
    return check_solver();
  }

  bool constraints_judgement_without_z3(FunctionPath *fp) {
    PEG::SmallPEGNodeSet null_nodes;
    PEG::SmallPEGNodeSet nonull_nodes;
    for (auto &cons_map : fp->constraints) {
      for (auto &cons : cons_map.second) {
        if (cons.lhs_node.size() != 1 ||
            cons.rhs_node.size() != 1)
          continue;
        for (auto lhs : cons.lhs_node) {
          for (auto rhs : cons.rhs_node) {
            if (lhs == rhs &&
                cons.op == llvm::CmpInst::Predicate::ICMP_NE) {
              return false;
            }
            if (lhs != rhs &&
                lhs->type == PEG::PEGNODE_TYPE::INT &&
                rhs->type == PEG::PEGNODE_TYPE::INT &&
                cons.op == llvm::CmpInst::Predicate::ICMP_EQ) {
              return false;
            }
            if (lhs->type != PEG::PEGNODE_TYPE::INT &&
                lhs->type != PEG::PEGNODE_TYPE::STATIC &&
                cons.op == llvm::CmpInst::Predicate::ICMP_EQ &&
                rhs->index == 0) {
              null_nodes.insert(lhs);
            }
            if (lhs->type != PEG::PEGNODE_TYPE::INT &&
                lhs->type != PEG::PEGNODE_TYPE::STATIC &&
                cons.op == llvm::CmpInst::Predicate::ICMP_NE &&
                rhs->index == 0) {
              nonull_nodes.insert(lhs);
            }
          }
        }
      }
    }

    for (auto &cons_map : fp->constraints) {
      for (auto &cons : cons_map.second) {
        if (cons.lhs_node.size() != 1 ||
            cons.rhs_node.size() != 1)
          continue;
        for (auto lhs : cons.lhs_node) {
          for (auto rhs : cons.rhs_node) {
            if (null_nodes.contains(lhs) &&
                null_nodes.contains(rhs) &&
                cons.op == llvm::CmpInst::Predicate::ICMP_NE) {
              return false;
            }
            if (null_nodes.contains(lhs) &&
                nonull_nodes.contains(rhs) &&
                cons.op == llvm::CmpInst::Predicate::ICMP_EQ) {
              return false;
            }
            if (null_nodes.contains(rhs) &&
                nonull_nodes.contains(lhs) &&
                cons.op == llvm::CmpInst::Predicate::ICMP_EQ) {
              return false;
            }
          }
        }
      }
    }

    return true;
  }
}
