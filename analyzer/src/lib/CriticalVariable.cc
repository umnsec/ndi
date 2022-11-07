#include "CriticalVariable.hh"
#include "PEGZ3.hh"

bool CriticalVariablePass::constitute_pathpair(FunctionPath *fp1,
                                               FunctionPath *fp2) {
  int diversing = 0, merging1 = 0, merging2 = 0;
  for (unsigned i = 1; i < fp1->path.size(); i++) {
    if (fp1->path[i] != fp2->path[i]) {
      diversing = i;
      break;
    }
  }

  for (unsigned i = diversing; i < fp1->path.size(); i++) {
    for (unsigned j = diversing; j < fp2->path.size(); j++) {
      if (fp1->path[i] == fp2->path[j]) {
        merging1 = i;
        merging2 = j;
        goto end_test;
      }
    }
  }

  end_test:
  if (fp1->path.size() - merging1 != fp2->path.size() - merging2)
    return false;
  for (unsigned i = merging1, j = merging2;
       i < fp1->path.size();
       i++,j++) {
    if (fp1->path[i] != fp2->path[j])
      return false;
  }
  return true;
}

void CriticalVariablePass::collect_release(FunctionPath *fp1,
                                           FunctionPath *fp2,
                                           PEG::SmallPEGNodeSet *release) {
  if (fp1->kfree_set == fp2->kfree_set) {
    return;
  }

  FunctionPath *set[2] = {fp1, fp2};
  for (int i = 0; i < 2; i++) {
    for (auto node : set[i]->kfree_set) {
      if (!set[1-i]->kfree_set.contains(node)) {
        if (set[1-i]->contains_node(node)) {
          PEG::SmallPEGNodeSet origin_set;
          if (set[i]->find_origin(node, &origin_set))
            continue;
          if (!set[1-i]->check_nodes(&origin_set)) {
            release->insert(node);
          }
        }
      }
    }
  }
}

bool CriticalVariablePass::fp_ret_null_check(FunctionPath *fp) {
  if (!fp->ret)
    return false;

  for (auto node : fp->alias_value[fp->ret]) {
    if (node->index == 0)
      return true;
    if (fp->point_to_value.count(node)) {
      if (fp->point_to_value[node].size() > 1)
        return false;
      for (auto point_to : fp->point_to_value[node]) {
        if (point_to->index != 0)
          return false;
      }
    }
    for (auto &cons : fp->constraints) {
      for (auto &curr : cons.second) {
        for (auto lhs : curr.lhs_node) {
          for (auto rhs : curr.rhs_node) {
          if ((lhs == node) &&
              (curr.op == llvm::CmpInst::ICMP_EQ) &&
              (rhs->index == 0)) {
            return true;
          }
          if ((rhs == node) &&
              (curr.op == llvm::CmpInst::ICMP_EQ) &&
              (lhs->index == 0)) {
            return true;
          }
          }
        }
      }
    }
  }
  return false;
}

bool CriticalVariablePass::fp_ret_nonnull_check(FunctionPath *fp) {
  if (!fp->ret)
    return false;
  for (auto node : fp->alias_value[fp->ret]) {
    for (auto &cons : fp->constraints) {
      for (auto &curr : cons.second)
      for (auto lhs : curr.lhs_node) {
        for (auto rhs : curr.rhs_node) {
          if ((lhs == node) &&
              (curr.op == llvm::CmpInst::ICMP_NE) &&
              (rhs->index == 0))
            return true;
          if ((rhs == node) &&
              (curr.op == llvm::CmpInst::ICMP_NE) &&
              (lhs->index == 0))
            return true;
        }
      }
    }
  }
  return false;
}

void CriticalVariablePass::collect_ret(FunctionPath *fp1,
                                       FunctionPath *fp2,
                                       PEG::SmallPEGNodeSet *ret) {
  if (!fp1->ret) // non return
    return;

  if (!Ctx->retval_fun_set.count(fp1->func))
    return;

  PEG::PEGNode *ret_node = fp1->ret;
  if (ret->contains(ret_node))
    return;

  if (fp_ret_null_check(fp1) &&
      fp_ret_nonnull_check(fp2)) {
    ret->insert(ret_node);
    return;
  }

  if (fp_ret_null_check(fp2) &&
      fp_ret_nonnull_check(fp1)) {
    ret->insert(ret_node);
    return;
  }
}

// placeholder version
void CriticalVariablePass::collect_init(FunctionPath *fp1,
                                        FunctionPath *fp2,
                                        PEG::SmallPEGNodeSet *init) {
  FunctionPath *set[2] = {fp1, fp2};
  for (int i = 0; i < 2; i++) {
    for (auto map : set[i]->point_to_value) {
      PEG::PEGNode *pointer = map.first;
      if (pointer->loc.type != PEG::location::ARGUMENT)
        continue;
      if (init->contains(pointer))
        continue;
      if (fp1->maybe_undefined_set.count(pointer) ||
          fp2->maybe_undefined_set.count(pointer))
        continue;
      if (set[i]->kfree_set.contains(pointer))
        continue;
      PEG::SmallPEGNodeSet *point_set1 = &map.second;
      // case 1: set[i] is placeholder and set[1-i] contains other values
      if (point_set1->size() == 1 &&
          set[i]->placeholder.count(pointer) &&
          point_set1->contains(set[i]->placeholder[pointer])) {
        if (set[1-i]->point_to_value.count(pointer)) {
          PEG::SmallPEGNodeSet *point_set2 = &set[1-i]->point_to_value[pointer];
          if (point_set2->size() > 1) {
            init->insert(pointer);
            continue;
          }
          if (set[1-i]->placeholder.count(pointer)) {
            if (!point_set2->contains(set[1-i]->placeholder[pointer])) {
              init->insert(pointer);
              continue;
            }
          }
        }
      } else {
        // case 2: set[i] contains other values and set[1-i] is placeholder
        if (!set[1-i]->point_to_value.count(pointer)) {
          init->insert(pointer);
          // TODO: if x->p = 1; we should add x->p to check list;
          continue;
        }

        PEG::SmallPEGNodeSet *point_set2 = &set[1-i]->point_to_value[pointer];
        if (point_set2->size() == 1 &&
            set[1-i]->placeholder.count(pointer) &&
            point_set2->contains(set[1-i]->placeholder[pointer])) {
          init->insert(pointer);
          continue;
        }
      }
    }

    PEG::SmallPEGNodeSet remove;
    for (auto origin : *init) {
      if (set[i]->placeholder.count(origin) &&
          init->contains(set[i]->placeholder[origin])) {
        remove.insert(origin);
      }
      if (set[i]->node_field_map.count(origin)) {
        for (auto tmp : set[i]->node_field_map[origin]) {
          if (tmp == origin)
            continue;
          if (init->contains(tmp)) {
            remove.insert(origin);
            break;
          }
        }
      }
    }
    for (auto r : remove) {
      init->erase(r);
    }
  }
}

void CriticalVariablePass::
collect_alias(PEG::SmallPEGNodeSet *cv_set,
              FunctionPath *fp,
              TargetFunctionState *tfs) {
  for (auto node : *cv_set) {
    tfs->cvs.insert(node);
    if (fp->alias_value.count(node)) {
      for (auto alias : fp->alias_value[node]) {
        tfs->cvs_alias.insert(alias);
      }
    }
    for (auto map : fp->alias_value) {
      if (map.second.count(node)) {
        tfs->cvs_alias.insert(map.first);
      }
    }
  }
}

bool CriticalVariablePass::doModulePass(llvm::Module *M) {
  for (auto &f : *M) {
    if (!f.empty() &&
        Ctx->plain_func_map.count(&f)) {
      //if (f.getName() != "search_requested_char") continue;
      OP << "Examing: " << f.getName() << "\n";
      FunctionPaths *fps = Ctx->plain_func_map[&f];
      Distinguisher dist_analyzer(false);
      FunctionPaths::iterator e = fps->end();
      FunctionPaths::iterator it = fps->begin();

      for (FunctionPaths::iterator it1 = fps->begin(); it1 != e; it1++) {
        FunctionPath *fp1 = *it1;
        for (FunctionPaths::iterator it2 = std::next(it1); it2 != e; it2++) {
          FunctionPath *fp2 = *it2;
          if (constitute_pathpair(fp1, fp2)) {
            if (Ctx->refcnt_fun_set.count(&f)) {
              if (fp1->retval == fp2->retval)
                continue;
            }
            PEG::SmallPEGNodeSet cv_set;
            bool flag = true;
            TargetFunctionState *tfs = &Ctx->func_pairs_map[&f];
#if CV_RELEASE
            // free security
            collect_release(fp1, fp2, &cv_set);
            if (!cv_set.empty()) {
              tfs->func = &f;
              if (flag){
                tfs->path_pairs.emplace(std::make_pair(fp1, fp2));
                flag = false;
              }
              collect_alias(&cv_set, fp1, tfs);
              collect_alias(&cv_set, fp2, tfs);
            }
            cv_set.clear();
#endif
#if CV_RET
            // Null state
            collect_ret(fp1, fp2, &cv_set);
            if (!cv_set.empty()) {
              dist_analyzer.retval = true;
              tfs->func = &f;
              if (flag) {
                tfs->path_pairs.emplace(std::make_pair(fp1, fp2));
                flag = false;
              }
              collect_alias(&cv_set, fp1, tfs);
              collect_alias(&cv_set, fp2, tfs);
            }
            cv_set.clear();
#endif
#if CV_INIT
            collect_init(fp1, fp2, &cv_set);
            if (!cv_set.empty()) {
              tfs->func = &f;
              if (flag) {
                tfs->path_pairs.emplace(std::make_pair(fp1, fp2));
                flag = false;
              }
              collect_alias(&cv_set, fp1, tfs);
              collect_alias(&cv_set, fp2, tfs);
            }
            cv_set.clear();
#endif
            if (!flag) {
              dist_analyzer.collect_distinguisher(fp1, fp2, &tfs->dists);
            }
          }
        }
      }
    }

    if (!Ctx->func_pairs_map[&f].cvs.empty()) {
      TargetFunctionState *tfs = &Ctx->func_pairs_map[&f];
      OP << KGRN << f.getName();
      OP << KNRM << ": " << tfs->path_pairs.size() << "\n";
      OP << "dist size: " << tfs->dists.size() << "\n";
      for (auto node : tfs->dists) {
        OP << *node << "\n";
      }
      checking_process(tfs, &f, false);
    }
  }
  return false;
}

void CriticalVariablePass::checking_process(TargetFunctionState *tfs,
                                            Function *f,
                                            bool add) {
  PEGFunctionBuilder builder(Ctx, f, &this->callee_count);
  unsigned env_num = builder.build_env();
  OP << "env functions number: " << env_num << "\n";
  if (env_num == 0) {
    return;
  }

  bool large_flag = false;
  OP << "cv size: " << tfs->cvs.size() << "\n";
  OP << "alias cv size: " << tfs->cvs_alias.size() << "\n";
  for (auto node : tfs->cvs) {
    OP << *node << "\n";
    OP << "=============\n";
    if (builder.identify_cv_alias(node) == -1) {
      OP << "Too large, escape\n";
      large_flag = true;
      break;
    }
  }
  if (large_flag)
    return;

  OP << "[1/4]: identifying uses of critical variables\n";
  builder.identify_cv_use(add);
  if (builder.root_cv_uses_map.empty()) {
    OP << "no cv use!\n";
    return;
  }
  OP << builder.root_cv_uses_map.size() << "\n";

  OP << "[2/4]: building dist environment\n";
  int dist = builder.build_dist_env();
  OP << "dist env functions number: " << dist << "\n";
  if (dist == 0) {
    OP << "no need to check dist use!\n";
    return;
  }

  OP << "[3/4]: identifying alias variables of dists\n";
  for (auto node : tfs->dists) {
    builder.identify_dist_without_cache(node);
  }

  OP << "[4/4]: identifying uses of dists\n";
  builder.identify_dist_use();
  if (!builder.judge()) {
    OP << KRED << "Caller error: no dist usage\n" << KNRM;
  }
}

bool CriticalVariablePass::doInitialization(llvm::Module *M) {
  return false;
}

bool CriticalVariablePass::doFinalization(llvm::Module *M) {
  return false;
}

