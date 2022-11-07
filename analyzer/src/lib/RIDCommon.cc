#include <queue>
#include <list>
#include <set>
#include <iostream>

#include "RIDCommon.hh"
#include "Common.hh"

using namespace llvm;

llvm::raw_ostream &operator<<(llvm::raw_ostream &out,
                              const Constraint &constraint) {
  out << KYEL << "CONSTRAINT: \n"  << KNRM;
  if (constraint.lhs)
    out << *constraint.lhs << "\n";
  else
    out << "Left NULL\n";
  OP << "------------------\n";
  for (auto node : constraint.lhs_node) {
    out << *node << "\n";
  }
  out << constraint.op << "\n";
  if (constraint.rhs)
    out << *constraint.rhs << "\n";
  else
    out << "Right NULL\n";
  OP << "------------------\n";
  for (auto node : constraint.rhs_node) {
    out << *node << "\n";
  }
  out << KYEL << "OVER" << KNRM;
  return out;
}

FunctionPath::FunctionPath(FunctionPath &fp) {
  args_set = fp.args_set;
  arg_node_map = fp.arg_node_map;
  node_field_map = fp.node_field_map;

  // this shold be null; but we keep it for historial reason
  constraints = fp.constraints;
  bb_direction = fp.bb_direction;

  func = fp.func;
  ret = fp.ret;

  path = fp.path;
  path_set = fp.path_set;

}

void FunctionPath::print_path() {
  OP << KMAG << "path " << ": ";
   for (BasicBlock *tmp : path) {
     tmp->printAsOperand(OP, false);
     OP << "->";
   }
   OP << "end\n" << KNRM;
}

bool FunctionPath::contains_node(PEG::PEGNode *node) {
  if (!(node->loc.type == PEG::location::LOCAL))
    return true;
  if (node->loc.loc_func != this->func)
    return false;
  if (!node->loc.loc_bb)
    return false;
  return this->path_set.contains(node->loc.loc_bb);
}

bool FunctionPath::check_node(PEG::PEGNode *node) {
  for (auto &map_cons : this->constraints) {
    for (auto &con : map_cons.second) {
      // Directly error check
      for (auto lhs : con.lhs_node) {
        if (lhs->type == PEG::PEGNODE_TYPE::CALL) {
          PEG::PEGCallNode *call_node =
            static_cast<PEG::PEGCallNode *>(lhs);
          if (call_node->callee == "IS_ERR" ||
              call_node->callee == "IS_ERR_OR_NULL" ||
              call_node->callee == "PTR_ERR_OR_ZERO") {
            if (callee_args_map[call_node][0].contains(node) &&
                con.op == llvm::CmpInst::ICMP_NE) {
              return true;
            }
          }
        }

        for (auto rhs : con.rhs_node) {
          if (lhs == node &&
              con.op == llvm::CmpInst::ICMP_EQ &&
              rhs->index == 0)
            return true;
          if (rhs == node &&
              con.op == llvm::CmpInst::ICMP_EQ &&
              lhs->index == 0)
            return true;
        }
      }
    }
  }
  return false;
}

bool FunctionPath::check_nodes(PEG::SmallPEGNodeSet *set) {
  for (auto node : *set) {
    if (check_node(node))
      return true;
  }
  return false;
}

bool FunctionPath::callee_involved(PEG::SmallPEGNodeSet *set) {
  PEG::SmallPEGNodeSet field_set;
  for (auto node : *set) {
    field_set.insert(node);
    for (auto map : this->point_to_value) {
      if (map.second.count(node))
        field_set.insert(map.first);
    }
  }

  PEG::SmallPEGNodeSet field_extend;
  for (auto cand : field_set) {
    if (node_field_map.count(cand)) {
      for (auto tmp : node_field_map[cand]) {
        field_extend.insert(tmp);
      }
    } else {
      for (auto &field_map : node_field_map) {
        if (field_map.second.count(cand)) {
          for (auto tmp : field_map.second) {
            field_extend.insert(tmp);
          }
          break;
        }
      }
    }
  }

  for (auto tmp : field_extend) {
    field_set.insert(tmp);
  }

  for (auto callee_map : this->callee_args_map) {
    PEG::PEGCallNode *callee_node = callee_map.first;

    bool up_flag = false;
    for (char c : callee_node->callee) {
      // remove UPPer function
      if (std::isupper(c)) {
        up_flag = true;
        break;
      }
    }
    if (up_flag)
      continue;

    if (callee_node->callee == "kfree" ||
        callee_node->callee == "CRYPTO_free" ||
        callee_node->callee == "free") {
      continue;
    }

    for (auto &arg_set : callee_map.second) {
      for (auto arg : arg_set) {
        if (field_set.count(arg)) {
          return true;
        }
        PEG::SmallPEGNodeSet point_to;
        this->find_point_to_set(arg, &point_to, false);
        for (auto node : *set) {
          if (point_to.contains(node))
            return true;
        }

        if (this->node_field_map.count(arg)) {
          for (auto arg_field :node_field_map[arg]) {
            point_to.clear();
            this->find_point_to_set(arg_field, &point_to, false);
            for (auto node : *set) {
              if (point_to.contains(node))
                return true;
            }
          }
        }
      }
    }
  }

  return false;
}

bool FunctionPath::global_involved(PEG::SmallPEGNodeSet *set) {
  for (auto node : *set) {
    if (!contains_node(node))
      return true;
    if (global_involved_set.count(node))
      return true;
  }
  return false;
}

void FunctionPath::update_phi_set() {
  BasicBlock *pre = nullptr;
  for (auto bb : this->path) {
    if (pre) {
      for (auto edge : *this->bb_edges_map[bb]) {
        if (edge->type == PEG::PEGEDGE_TYPE::PHI) {
          PEG::PEGPhiNode *phi_node =
            static_cast<PEG::PEGPhiNode *>(edge->dst);
          if (phi_node->incomings[pre] == edge->src) {
            this->phi_set.insert(edge);
          }
        }
      }
    }
    pre = bb;
  }
}

bool FunctionPath::find_point_to_set(PEG::PEGNode *node,
                                     PEG::SmallPEGNodeSet *point_to_set,
                                     bool remove_placeholder) {
  if (this->point_to_value.count(node) ||
      this->node_field_map.count(node)) {
    std::list<PEG::PEGNode *> node_list;
    node_list.push_back(node);
    point_to_set->insert(node);
    while (!node_list.empty()) {
      PEG::PEGNode *front = node_list.front();
      node_list.pop_front();

      if (this->point_to_value.count(front)) {
        for (auto value : this->point_to_value[front]) {
          if (!point_to_set->count(value)) {
            node_list.push_back(value);
            point_to_set->insert(value);
          }
        }
      }

      if (this->node_field_map.count(front)) {
        for (auto value : this->node_field_map[front]) {
          if (!point_to_set->count(value)) {
            node_list.push_back(value);
            point_to_set->insert(value);
          }
        }
      }
    }
    if (remove_placeholder) {
    point_to_set->erase(node);
    if (this->placeholder.count(node))
      point_to_set->erase(this->placeholder[node]);
    }
    return !point_to_set->empty();
  } else {
    return false;
  }
}

void FunctionPath::update_global_value() {
  for (auto map : this->point_to_value) {
    PEG::PEGNode *pointer = map.first;
    if (pointer->loc.type != PEG::location::LOCAL) {
      this->find_point_to_set(pointer,
                              &this->global_value_map[pointer],
                              false);
    }
  }

  if (this->ret) {
    retval.insert(ret);
    if (this->alias_value.count(ret)) {
      for (auto node : this->alias_value[ret]) {
        retval.insert(node);
        this->find_point_to_set(node, &retval, false);
      }
    }
    for (auto node : retval) {
      if (node->type == PEG::PEGNODE_TYPE::FIELD) {
        PEG::PEGNode *base_node =
          static_cast<PEG::PEGFieldNode *>(node)->pointer;
        global_involved_set.insert(base_node);
      }
    }
  } else {
    retval.clear();
  }
}

void FunctionPath::
find_virtualnode_backwords(PEG::PEGNode *node,
                           PEG::PEGVirtualNodesList *virtual_nodes,
                           PEG::PEGVirtualEdgeList *path,
                           PEG::SmallPEGNodeSet *nodes_set,
                           bool print) {
  if (print) {
    OP << *node << "\n";
  }
  if (node->top) {
    PEG::PEGVirtualNode *virtual_node = new PEG::PEGVirtualNode(node);
    virtual_node->path = *path;
    virtual_nodes->push_back(virtual_node);
    return;
  }
  if (node->type == PEG::PEGNODE_TYPE::FIELD) {
    PEG::PEGFieldNode *field_node = static_cast<PEG::PEGFieldNode *>(node);
    path->emplace_front(PEG::PEGVIRTUALEDGE_TYPE::FIELD,
                        field_node->offset);
    nodes_set->insert(field_node->pointer);
    find_virtualnode_backwords(field_node->pointer, virtual_nodes, path,
                               nodes_set, print);
    nodes_set->erase(field_node->pointer);
    path->pop_front();

    for (auto map : this->node_field_map) {
      if (map.first == node)
        continue;
      if (!map.second.count(node))
        continue;
      if (map.first == field_node->pointer)
        continue;
      if (nodes_set->count(map.first))
        continue;
      path->emplace_front(PEG::PEGVIRTUALEDGE_TYPE::FIELD,
                          field_node->offset);
      nodes_set->insert(map.first);
      find_virtualnode_backwords(map.first, virtual_nodes, path,
                                 nodes_set, print);
      nodes_set->erase(map.first);
      path->pop_front();
    }
  } else {
    if (this->alias_value.count(node)) {
      for (auto alias : this->alias_value[node]) {
        if (alias == node)
          continue;
        if (nodes_set->count(alias))
          continue;

        nodes_set->insert(alias);
        find_virtualnode_backwords(alias, virtual_nodes, path,
                                   nodes_set, print);
        nodes_set->erase(alias);
      }
    }
    for (auto map : this->point_to_value) {
      if (!map.second.count(node))
        continue;
      if (map.first == node)
        continue;
      if (nodes_set->count(map.first))
        continue;
      path->emplace_front(PEG::PEGVIRTUALEDGE_TYPE::LOAD, 0);
      nodes_set->insert(map.first);
      find_virtualnode_backwords(map.first, virtual_nodes, path,
                                 nodes_set, print);
      nodes_set->erase(map.first);
      path->pop_front();
    }

    for (auto map : this->placeholder) {
      if (map.second == node) {
        path->emplace_front(PEG::PEGVIRTUALEDGE_TYPE::LOAD, 0);
        nodes_set->insert(map.first);
        find_virtualnode_backwords(map.first, virtual_nodes, path,
                                   nodes_set, print);
        nodes_set->erase(map.first);
        path->pop_front();
      }
    }
  }
}

PEG::PEGVirtualNodesList *
FunctionPath::get_virtualnodes(PEG::PEGNode *node) {
  if (this->node_virtual_map.count(node)) {
    if (this->node_virtual_map[node].size() == 0)
      return nullptr;
    return &this->node_virtual_map[node];
  }

  if (node == ret) {
    if (!node_virtual_map.count(node))
      this->node_virtual_map[node].push_back(new PEG::PEGVirtualNode(this->ret));
    return &this->node_virtual_map[node];
  }

  PEG::PEGVirtualNodesList *virtual_nodes = &this->node_virtual_map[node];
  PEG::PEGVirtualEdgeList path;
  PEG::SmallPEGNodeSet nodes_set;
  if (this->retval.contains(node)) {
    if (!this->node_virtual_map.count(ret)) {
      this->node_virtual_map[ret].push_back(new PEG::PEGVirtualNode(this->ret));
    }
    for (auto ret_node : this->node_virtual_map[ret])
      virtual_nodes->push_back(ret_node);
  }

  nodes_set.insert(node);
  find_virtualnode_backwords(node, virtual_nodes, &path,
                             &nodes_set, node->index == 208);

  if (virtual_nodes->size() == 0) {
    return nullptr;
  }

  return virtual_nodes;
}

bool FunctionPath::find_origin(PEG::PEGNode *node,
                               PEG::SmallPEGNodeSet *set) {
  set->insert(node);
  std::list<PEG::PEGNode *> list;
  list.push_back(node);
  bool flag = false;
  PEG::PEGNode *front = nullptr;
  while (!list.empty()) {
    front = list.front();
    list.pop_front();

    for (auto map : this->placeholder) {
      if (map.second == front) {
        if (!set->contains(map.first)) {
          list.push_back(map.first);
          set->insert(map.first);
        }
      }
    }
    if (front->type == PEG::PEGNODE_TYPE::FIELD) {
      for (auto map : this->node_field_map) {
        if (map.second.contains(front)) {
          PEG::PEGNode *pointer = map.first;
          if (!set->contains(pointer)) {
            list.push_back(pointer);
            set->insert(pointer);
          }
        }
      }
    }
  }

  if (front->loc.type == PEG::location::LOCAL &&
      front->loc.loc_bb == &front->loc.loc_func->getEntryBlock() &&
      front->type != PEG::PEGNODE_TYPE::CALL)
    flag = true;

  // if the origin is kzalloc(), which means
  // it should be null (load from of null).
  if (front != node &&
      front->type == PEG::PEGNODE_TYPE::CALL &&
      static_cast<PEG::PEGCallNode *>(front)->callee == "kzalloc")
    flag = true;

  return flag;
}

bool FunctionPath::listdel_involved(PEG::SmallPEGNodeSet *set) {
  if (this->listdel_set.empty())
    return false;

  for (auto node : *set) {
    if (this->node_field_map.count(node)) {
      for (auto del : this->listdel_set) {
        if (this->node_field_map[node].count(del)) {
          return true;
        }
      }
    }
  }
  return false;
}

bool FunctionPath::retval_involved(PEG::SmallPEGNodeSet *set) {
  for (auto node : *set) {
    if (this->retval.contains(node))
      return true;
  }
  return false;
}

