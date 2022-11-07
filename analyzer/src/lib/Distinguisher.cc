#include "Distinguisher.hh"
#include "PEGZ3.hh"

bool Distinguisher::
collect_retval(FunctionPath *fp1, FunctionPath *fp2,
               PEG::SmallPEGNodeSet *dists) {
  if (!fp1->ret) // void, of course not
    return false;

  if (dists->count(fp1->ret))
    return true;

  if (fp1->retval == fp2->retval) {
    PEGZ3::clear_solver();
    for (auto node : fp1->retval) {
      PEGZ3::add_functionpath_pegnode(fp1, node);
      PEGZ3::add_functionpath_pegnode(fp2, node);
    }
    if (!PEGZ3::check_solver()) {
      dists->insert(fp1->ret);
      return true;
    }
    return false;
  }

  dists->insert(fp1->ret);
  return true;
}

bool Distinguisher::
collect_global(FunctionPath *fp1, FunctionPath *fp2,
               PEG::SmallPEGNodeSet *dists) {
  FunctionPath *set[2] = {fp1, fp2};
  bool ret_flag = false;
  for (int i = 0; i < 2; i++) {
    for (auto map : set[i]->global_value_map) {
      PEG::PEGNode *pointer = map.first;
      if (dists->count(pointer))
        continue;
      PEG::SmallPEGNodeSet set0_point_to_set(map.second);
      set0_point_to_set.erase(pointer);
      if (set[i]->placeholder.count(pointer))
        set0_point_to_set.erase(set[i]->placeholder[pointer]);
      if (set[i]->node_field_map.count(pointer)) {
        for (auto node : set[i]->node_field_map[pointer]) {
          set0_point_to_set.erase(node);
          if (set[i]->placeholder.count(node))
            set0_point_to_set.erase(set[i]->placeholder[node]);
        }
      }
      if (!set0_point_to_set.empty()) {
        if (!set[1-i]->global_value_map.count(pointer)) {
          dists->insert(pointer);
          continue;
        }
        PEG::SmallPEGNodeSet
          set1_point_to_set(set[1-i]->global_value_map[pointer]);
        set1_point_to_set.erase(pointer);
        if (set[1-i]->placeholder.count(pointer))
          set1_point_to_set.erase(set[1-i]->placeholder[pointer]);
        if (set[1-i]->node_field_map.count(pointer)) {
          for (auto node : set[1-i]->node_field_map[pointer]) {
            set1_point_to_set.erase(node);
            if (set[1-i]->placeholder.count(node))
              set1_point_to_set.erase(set[1-i]->placeholder[node]);
          }
        }
        if (set0_point_to_set != set1_point_to_set) {
          dists->insert(pointer);
          continue;
        }
      }
    }
  }
  return ret_flag;
}

bool Distinguisher::
collect_condition(FunctionPath *fp1, FunctionPath *fp2,
                  PEG::SmallPEGNodeSet *dists) {
  FunctionPath *set[2] = {fp1, fp2};
  std::map<PEG::PEGNode *, PEG::SmallPEGNodeSet> node_value_set;
  for (int i = 0; i < 2; i++) {
    for (auto map : set[i]->global_value_map) {
      for (auto value : map.second) {
        node_value_set[map.first].insert(value);
      }
    }
  }

  llvm::DenseMap<PEG::PEGNode *, PEG::SmallPEGNodeSet> global_condition;
  for (int i = 0; i < 2; i++) {
    for (auto &cons_map : set[i]->constraints) {
      for (auto &con : cons_map.second) {
        const PEG::SmallPEGNodeSet *nodes[2] =
          {&con.lhs_node, &con.rhs_node};
        for (int j = 0; j < 2; j++) {
          for (auto node : *nodes[j]) {
            if (global_condition.count(node))
              continue;
            if (node->type == PEG::PEGNODE_TYPE::STATIC ||
                node->type == PEG::PEGNODE_TYPE::INT)
              continue;
            if (node->type == PEG::PEGNODE_TYPE::OBJECT &&
                node->loc.type == PEG::location::ARGUMENT)
              global_condition[node].insert(node);
            else {
              for (auto value_map : node_value_set) {
                if (value_map.second.contains(node)) {
                  global_condition[node].insert(value_map.first);
                }
              }
              if (global_condition[node].empty()) {
                if (node->type != PEG::PEGNODE_TYPE::CALL)
                  global_condition[node].insert(node);
              }
            }
          }
        }
      }
    }
  }

  bool ret_flag = false;
  for (auto cond_map : global_condition) {
    bool need_flag = true;
    // if all the pointer has already been collected, there is no
    // need to test again
    for (auto pointer : cond_map.second) {
      if (!dists->count(pointer)) {
        need_flag = false;
        break;
      }
    }
    if (need_flag)
      continue;

    if (!opt_map.count(cond_map.first))
      opt_map[cond_map.first] = 0;
    else {
      if (opt_map[cond_map.first] > 100)
        continue;
      opt_map[cond_map.first]++;
    }

    PEGZ3::clear_solver();
    PEGZ3::add_functionpath_pegnode(fp1, cond_map.first);
    PEGZ3::add_functionpath_pegnode(fp2, cond_map.first);
    if (!PEGZ3::check_solver()) {
      ret_flag = true;
      for (auto pointer : cond_map.second) {
        dists->insert(pointer);
      }
    }
  }
  return ret_flag;
}


bool Distinguisher::collect_distinguisher(FunctionPath *fp1, FunctionPath *fp2,
                                          PEG::SmallPEGNodeSet *dists) {
  // collect retnode
  bool ret_flag, global_flag, condition_flag;
  if (!retval)
   ret_flag = collect_retval(fp1, fp2, dists);
  else {
    ret_flag = true;
    dists->insert(fp1->ret);
  }
  global_flag = collect_global(fp1, fp2, dists);
  condition_flag = collect_condition(fp1, fp2, dists);
  if (fp1->ret)
    dists->insert(fp1->ret);
  return ret_flag || global_flag || condition_flag;
}
