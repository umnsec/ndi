#include "ValueFlowPass.hh"

void ValueFlowPass::set_alias_value(FunctionPath *fp,
                                    PEG::PEGNode *node,
                                    PEG::SmallPEGNodeSet *location) {
  if (fp->alias_value.count(node)) {
    *location = fp->alias_value[node];
  } else {
    location->insert(node);
  }
}

void ValueFlowPass::
collect_constraints(FunctionPath *fp, BasicBlock *curr,
                    PEG::PEGEdge *cond_edge) {
  if (!fp->bb_direction.count(curr))
    return;

  PEG::PEGNode *src = cond_edge->src;
  PEG::PEGBrNode *dst =
    static_cast<PEG::PEGBrNode *>(cond_edge->dst);
  if (src->type == PEG::PEGNODE_TYPE::CMP) {
    PEG::PEGCmpNode *cmp_node = static_cast<PEG::PEGCmpNode *>(src);
    Constraint con;
    con.lhs = cmp_node->lhs;
    set_alias_value(fp, cmp_node->lhs, &con.lhs_node);
    con.rhs = cmp_node->rhs;
    set_alias_value(fp, cmp_node->rhs, &con.rhs_node);
    con.op = fp->bb_direction[curr]?
      cmp_node->op:
      llvm::CmpInst::getInversePredicate(cmp_node->op);
    fp->constraints[curr].push_back(con);
    return;
  }

  Constraint con;
  con.lhs = src;
  set_alias_value(fp, src, &con.lhs_node);
  con.rhs = PEG::int_pegintnode_map[0];
  con.rhs_node.insert(con.rhs);
  con.op = fp->bb_direction[curr] ?
    llvm::CmpInst::Predicate::ICMP_EQ:
    llvm::CmpInst::Predicate::ICMP_NE;
  fp->constraints[curr].push_back(con);
  return;
  OP << KRED << *cond_edge << " unhandled \n" << KNRM;
  // handle select
}

bool ValueFlowPass::translate_edge(FunctionPath *fp,
                                   BasicBlock *curr,
                                   PEG::PEGEdge *edge) {
  PEG::PEGNode *src = edge->src;
  PEG::PEGNode *dst = edge->dst;

  if (edge->type == PEG::PEGEDGE_TYPE::PHI) {
    if (!fp->phi_set.contains(edge)) {
      return true;
    }
  }

  if (edge->type != PEG::PEGEDGE_TYPE::STORE) {
    if (src->loc.type != PEG::location::LOCAL) {
      fp->global_involved_set.insert(src);
    }
    if (fp->global_involved_set.count(src)) {
      if (edge->type == PEG::PEGEDGE_TYPE::ARG) {
        PEG::PEGCallNode *call_node =
          static_cast<PEG::PEGCallNode *>(dst);
        // origin code is call_node->callee != "memdup_user" && etc
        // we should replace with global function list
        if (src->type != PEG::PEGNODE_TYPE::INT &&
            src->type != PEG::PEGNODE_TYPE::STATIC) {
          // do we need to add object here? still in question
          fp->global_involved_set.insert(dst);

          for (auto node : call_node->args) {
            if (node->type != PEG::PEGNODE_TYPE::INT &&
                node->type != PEG::PEGNODE_TYPE::STATIC &&
                node->type != PEG::PEGNODE_TYPE::OBJECT) {
              fp->global_involved_set.insert(node);
              if (fp->alias_value.count(node)) {
                for (auto alias : fp->alias_value[node]) {
                  if (alias->type != PEG::PEGNODE_TYPE::INT &&
                      alias->type != PEG::PEGNODE_TYPE::STATIC &&
                      alias->type != PEG::PEGNODE_TYPE::OBJECT) {
                    fp->global_involved_set.insert(alias);
                  }
                }
              }
            }
          }
        }
      } else {
        fp->global_involved_set.insert(dst);
      }
    }
  }

  if (edge->type == PEG::PEGEDGE_TYPE::STORE) {
    PEG::SmallPEGNodeSet pointer_set;
    PEG::SmallPEGNodeSet value_set;
    set_alias_value(fp, src, &value_set);
    set_alias_value(fp, dst, &pointer_set);
    for (auto pointer : pointer_set) {
      if (pointer->type == PEG::PEGNODE_TYPE::STATIC ||
          pointer->type == PEG::PEGNODE_TYPE::INT ||
          pointer->type == PEG::PEGNODE_TYPE::OBJECT)
        continue;
      if (pointer->loc.type == PEG::location::ARGUMENT &&
          src->loc.type == PEG::location::LOCAL &&
          fp->global_involved_set.count(src) &&
          src->type != PEG::PEGNODE_TYPE::CALL &&
          src->type != PEG::PEGNODE_TYPE::OBJECT) {
        fp->maybe_undefined_set.insert(pointer);
      }
      for (auto value : value_set) {
        if (!pointer->partly) {
          for (auto node : fp->point_to_value[pointer]) {
            if (fp->kfree_set.contains(node))
              fp->kfree_set.erase(node);
          }
          fp->point_to_value[pointer].clear();
        }

        if (pointer->loc.type != PEG::location::LOCAL) {
          fp->global_involved_set.insert(pointer);
        }
        if (fp->global_involved_set.count(pointer)) {
          fp->global_involved_set.insert(value);
          PEG::SmallPEGNodeSet value_involved;
          fp->find_point_to_set(value, &value_involved, false);
          for (auto node : value_involved)
            fp->global_involved_set.insert(node);
        }

        fp->point_to_value[pointer].insert(value);
        if (fp->placeholder.count(pointer) &&
            fp->kfree_set.count(fp->placeholder[pointer]))
          fp->kfree_set.erase(fp->placeholder[pointer]);
      }
    }

    return true;
  }

  if (edge->type == PEG::PEGEDGE_TYPE::LOAD) {
    if (fp->alias_value.count(src)) {
      // case like load from phi
      for (auto node : fp->alias_value[src]) {
        if (!fp->point_to_value.count(node)) {
          // there is no value, the load itself represents it
          // you can not load from int or static
          if (node->type == PEG::PEGNODE_TYPE::STATIC ||
              node->type == PEG::PEGNODE_TYPE::INT ||
              node->type == PEG::PEGNODE_TYPE::OBJECT)
            continue;
          fp->point_to_value[node].insert(dst);
          fp->placeholder[node] = dst;
        } else {
          for (auto point_to : fp->point_to_value[node]) {
            fp->alias_value[dst].insert(point_to);
          }
        }
      }
    } else {
      if (!fp->point_to_value.count(src)) {
        // there is no value, the load itself represents it
        fp->point_to_value[src].insert(dst);
        fp->placeholder[src] = dst;
      } else {
        fp->alias_value[dst] = fp->point_to_value[src];
      }
    }
    return true;
  }

  if (edge->type == PEG::PEGEDGE_TYPE::FIELD) {
    if (fp->alias_value.count(src)) {
      for (auto node : fp->alias_value[src]) {
        if (node->type == PEG::PEGNODE_TYPE::STATIC ||
            node->type == PEG::PEGNODE_TYPE::INT)
          continue;

        if (!fp->node_field_map.count(node))
          fp->node_field_map[node].insert(node);
        fp->node_field_map[node].insert(dst);
      }
    } else {
      if (!fp->node_field_map.count(src))
        fp->node_field_map[src].insert(src);
      fp->node_field_map[src].insert(dst);
    }
    return true;
  }

  if (edge->type == PEG::PEGEDGE_TYPE::ASSIGN) {
    set_alias_value(fp, src, &fp->alias_value[dst]);
    if (fp->alias_value.count(src)) {
      for (auto node : fp->alias_value[src]) {
        fp->alias_value[dst].insert(node);
      }
    } else {
      fp->alias_value[dst].insert(src);
    }
    return true;
  }

  if (edge->type == PEG::PEGEDGE_TYPE::PHI) {
    PEG::PEGPhiNode *phi_node = static_cast<PEG::PEGPhiNode *>(dst);
    set_alias_value(fp, src, &fp->alias_value[phi_node]);
    return true;
  }

  if (edge->type == PEG::PEGEDGE_TYPE::CMP) {
    return true;
  }

  if (edge->type == PEG::PEGEDGE_TYPE::RESERVE) {
    PEG::PEGCallNode *call_node =
      static_cast<PEG::PEGCallNode *>(dst);
    if (call_node->callee.contains("memset")) {
      for (auto node : fp->callee_args_map[call_node][0]) {
        fp->init_set.insert(node);
      }
    }
    return true;
  }

  if (edge->type == PEG::PEGEDGE_TYPE::COND) {
    // handle select situation
    collect_constraints(fp, curr, edge);
    return true;
  }

  if (edge->type == PEG::PEGEDGE_TYPE::BINARY) {
    return true;
  }

  if (edge->type == PEG::PEGEDGE_TYPE::ARG) {
    PEG::PEGCallNode *call_node = static_cast<PEG::PEGCallNode *>(dst);
    PEG::SmallPEGNodeSet arg_set;
    set_alias_value(fp, src, &arg_set);
    fp->callee_args_map[call_node].push_back(arg_set);
    if (Ctx->release_funcs.count(call_node->callee.str())) {
      if (*call_node->args.begin() != src)
        return true;

      if (src->type == PEG::PEGNODE_TYPE::STATIC ||
          src->type == PEG::PEGNODE_TYPE::INT)
        return true;

      PEG::SmallPEGNodeSet kfree_values;
      if (fp->alias_value.count(src)) {
        for (auto node : fp->alias_value[src]) {
          if (node->type == PEG::PEGNODE_TYPE::STATIC ||
              node->type == PEG::PEGNODE_TYPE::INT)
            continue;
          kfree_values.insert(node);
        }
      } else {
        kfree_values.insert(src);
      }

      if (!fp->listdel_involved(&kfree_values)) {
        for (auto node : kfree_values) {
          fp->kfree_set.insert(node);
        }
      }
    }
    return true;
  }
  return false;
}


void ValueFlowPass::translate_functionpath(FunctionPath *fp) {
  for (auto bb : fp->path) {
    for (auto edge : *fp->bb_edges_map[bb]) {
      if (!translate_edge(fp, bb, edge)) {
        OP << *edge << KRED << " can not be translated to value map"
           << KNRM << "\n";
        return;
      }
    }
  }
  fp->update_global_value();
}

bool ValueFlowPass::doModulePass(llvm::Module *M) {
  for (auto &f : *M) {
    if (!f.empty() &&
        Ctx->plain_func_map.count(&f) &&
        Ctx->callee_set.count(&f)) {
      OP << f.getName() << "\n";
      FunctionPaths *fps = Ctx->plain_func_map[&f];
      for (auto fp : *fps) {
        fp->point_to_value.clear();
        fp->alias_value.clear();
        translate_functionpath(fp);
      }
    }
  }
  return false;
}

bool ValueFlowPass::doInitialization(llvm::Module *M) {
  return false;
}

bool ValueFlowPass::doFinalization(llvm::Module *M) {
  return false;
}
