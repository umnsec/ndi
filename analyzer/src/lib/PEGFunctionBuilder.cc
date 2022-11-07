#include "llvm/IR/InstIterator.h"

#include "PEGFunctionBuilder.hh"
#include "CriticalVariable.hh"
#include "Tools.hh"

PEGFunctionBuilder::~PEGFunctionBuilder() {
  for (auto inform : this->func_peg_inform) {
    for (auto map : inform->peg_node_map) {
      if (map.first != map.second)
        delete map.second;
    }
    for (auto map : inform->bb_edges_map) {
      for (auto edge : map.second)
        delete edge;
    }
    for (auto map : inform->links) {
      for (auto edge : map.second)
        delete edge;
    }
    delete inform;
  }
}

void PEGFunctionBuilder::print_node_inform(PEG::PEGNode *node) {
  for (auto edge : src_map[node]) {
    OP << *edge << "\n";
  }
  for (auto edge : dst_map[node]) {
    OP << *edge << "\n";
  }
}

unsigned PEGFunctionBuilder::count_callees(Function *func, FuncSet *path) {
  if (callee_count->count(func))
    return (*callee_count)[func];

  path->insert(func);
  unsigned num = 1;
  for (inst_iterator i = inst_begin(func), e = inst_end(func);
       i != e; ++i) {
    if (CallInst *call_ins = dyn_cast<CallInst>(&*i)) {
      if (call_ins->isIndirectCall())
        continue;
      for (auto callee : ctx->Callees[call_ins]) {
        if (!callee)
          continue;
        if (!path->contains(callee)) {
          if (callee_count->count(func))
            num += (*callee_count)[func];
          else
            num += count_callees(callee, path);
        }
      }
    }
  }
  path->erase(func);
  (*callee_count)[func] = num;
  return num;
}

FunctionPEGInform *PEGFunctionBuilder::
build_func_peg_inform(Function *func) {
  FunctionPEGInform *inform = new FunctionPEGInform;
  inform->func = func;
  for (auto& arg : func->args()) {
    inform->arg_node_map[&arg] = PEG::copy_node(ctx->arg_node_map[&arg]);
    inform->peg_node_map[ctx->arg_node_map[&arg]] =
      inform->arg_node_map[&arg];
  }
  if (ctx->func_retnode_map.count(func)) {
    inform->retnode = PEG::copy_node(ctx->func_retnode_map[func]);
    inform->peg_node_map[ctx->func_retnode_map[func]] = inform->retnode;
  } else {
    inform->retnode = nullptr;
  }
  for (auto& bb : *func) {
    for (auto edge : ctx->bb_edges_map[&bb]) {
      if (!inform->peg_node_map.count(edge->src)) {
        inform->peg_node_map[edge->src] = PEG::copy_node(edge->src);
      }
      if (!inform->peg_node_map.count(edge->dst)) {
        inform->peg_node_map[edge->dst] = PEG::copy_node(edge->dst);
      }
      PEG::PEGEdge *new_edge =
        new PEG::PEGEdge(inform->peg_node_map[edge->src],
                         inform->peg_node_map[edge->dst],
                         edge->type);
      inform->bb_edges_map[&bb].push_back(new_edge);
      inform->edge_bb_map[new_edge] = &bb;
    }
  }

  for (auto map : inform->peg_node_map) {
    if (map.first->type == PEG::PEGNODE_TYPE::CALL) {
      PEG::PEGCallNode *first_node =
        static_cast<PEG::PEGCallNode *>(map.first);
      PEG::PEGCallNode *second_node =
        static_cast<PEG::PEGCallNode *>(map.second);
      if (this->ctx->callnode_funcs_map.count(first_node))
        inform->callnode_funcs_map[second_node] =
          this->ctx->callnode_funcs_map[first_node];
    }
  }

  this->func_peg_inform.insert(inform);
  return inform;
}

FunctionPEGInform *PEGFunctionBuilder::
add_links(Function *func, FuncSet *path, int level) {
  FunctionPEGInform *inform = build_func_peg_inform(func);
  if (func == root_func)
    root_inform.insert(inform);
  if (level == 0)
    return inform;
  path->insert(func);
  std::vector<PEG::PEGEdge *> link;
  for (auto& bb : *func) {
    for (auto edge : inform->bb_edges_map[&bb]) {

      if (edge->type == PEG::PEGEDGE_TYPE::ARG) {
        link.push_back(edge);
      }

      if (edge->type == PEG::PEGEDGE_TYPE::RESERVE) {
        PEG::PEGCallNode *callee_node =
          static_cast<PEG::PEGCallNode *>(edge->dst);

        if (!inform->callnode_funcs_map.count(callee_node)) {
          link.clear();
          continue;
        }
        for (auto callee : *inform->callnode_funcs_map[callee_node]) {
          if (path->count(callee)) {
            continue;
          }

          if (callee_node->indirect)
            continue;

          FunctionPEGInform *callee_inform =
            add_links(callee, path, level - 1);
          inform->inform_callee_map[callee_inform] = edge;
          callee_inform->parent = inform;
          for (unsigned i = 0; i < callee->arg_size(); i++) {
            inform->links[edge].
              push_back(new
                        PEG::PEGEdge(link[i]->src,
                                     callee_inform->
                                     arg_node_map[callee->getArg(i)],
                                     PEG::PEGEDGE_TYPE::ASSIGN));
          }
          if (ctx->func_retnode_map.count(callee)) {
            inform->links[edge].
              push_back(new
                        PEG::PEGEdge(callee_inform->retnode,
                                     callee_node,
                                     PEG::PEGEDGE_TYPE::ASSIGN));
          }
        }
        link.clear();
      }
    }
  }
  path->erase(func);
  return inform;
}

void PEGFunctionBuilder::update_graph_inform(FunctionPEGInform *inform) {
  for (auto& bb : *inform->func) {
    for (auto edge : inform->bb_edges_map[&bb]) {
      this->src_map[edge->src].push_back(edge);
      this->dst_map[edge->dst].push_back(edge);
      this->edge_map[edge] = inform;
    }
  }
  for (auto link : inform->links) {
    for (auto edge : link.second) {
      this->src_map[edge->src].push_back(edge);
      this->dst_map[edge->dst].push_back(edge);
      this->edge_map[edge] = this->edge_map[link.first];
    }
  }
}

void PEGFunctionBuilder::build_graph_inform() {
  for (auto inform : this->func_peg_inform) {
    this->update_graph_inform(inform);
  }
}

int PEGFunctionBuilder::build_env() {
  FuncSet pre;
  FuncSet after;
  std::map<Function *, int> top_funcs;

  if (!ctx->Callers.count(root_func))
    return 0;

  pre.insert(root_func);
  top_funcs[root_func] = 0;

  // identify k-limit call stack
  for (unsigned i = 0; i < KLIMIT; i++) {
    for (auto func : pre) {
      if (!ctx->Callers.count(func)) {
        continue;
      }
      bool top_flag = true;
      for (auto caller_ins : ctx->Callers[func]) {

        if (caller_ins->isIndirectCall() &&
            (caller_ins->arg_size() < 3)) {
          continue;
        }

        top_flag = false;
        Function *caller = caller_ins->getFunction();
        top_funcs[caller] = i + 1;
        after.insert(caller);
      }
      if (!top_flag)
        top_funcs[func] = -1;
    }
    pre = after;
    after.clear();
  }

  FuncSet call_path;
  int num = 0;
  for (auto map : top_funcs) {
    if (map.second <= 0)
      continue;
    Function *func = map.first;
    this->top_inform[map.first] = add_links(func, &call_path, map.second);
    this->top_inform[func]->parent = nullptr;
    num += count_callees(func, &call_path);
    if (call_path.size() != 0)
      OP << KRED << "ERR in add_links for top function\n" << KNRM;
  }

  build_graph_inform();
  return num;
}

PEG::PEGEdge *PEGFunctionBuilder::
collect_related_args(FunctionPEGInform *inform,
                     std::vector<PEG::PEGEdge *> *link,
                     PEG::PEGCallNode *callee_node) {
  for (auto& bb : *inform->func) {
    for (auto edge : inform->bb_edges_map[&bb]) {
      if (edge->type == PEG::PEGEDGE_TYPE::ARG) {
        if (callee_node == edge->dst)
          link->push_back(edge);
      }
      if (edge->type == PEG::PEGEDGE_TYPE::RESERVE) {
        PEG::PEGCallNode *dst =
          static_cast<PEG::PEGCallNode *>(edge->dst);
        if (dst == callee_node)
          return edge;
      }
    }
  }

  OP << KRED << "we should not reach here: collect_related_args()\n" << KNRM;
  return nullptr;
}


void PEGFunctionBuilder::
update_caller_callee_inform(FunctionPEGInform *caller_inform,
                            PEG::PEGCallNode *callee_node) {
  if (callee_node->indirect)
    return;
  FuncSet path;
  FunctionPEGInform *tmp = caller_inform;
  while (tmp != nullptr) {
    path.insert(tmp->func);
    tmp = tmp->parent;
  }

  std::vector<PEG::PEGEdge *> link;
  PEG::PEGEdge *callee_edge =
    collect_related_args(caller_inform, &link, callee_node);

  for (auto map : caller_inform->inform_callee_map) {
    if (map.second == callee_edge) {
      if (!this->dist_peg_inform.empty() &&
          !this->dist_peg_inform.count(map.first)) {
        this->dist_peg_inform.insert(map.first);
        update_graph_inform(map.first);
        if (!caller_inform->links.count(callee_edge)) {
          OP << KRED << "ERR: no callee links but have calle inform\n" << KNRM;
        }
        for (auto edge : caller_inform->links[callee_edge]) {
          this->src_map[edge->src].push_back(edge);
          this->dst_map[edge->dst].push_back(edge);
          this->edge_map[edge] = this->edge_map[callee_edge];
        }
      }
      return;
    }
  }

  if (!caller_inform->callnode_funcs_map.count(callee_node)) {
    return;
  }

  for (auto callee : *caller_inform->callnode_funcs_map[callee_node]) {
    if (path.count(callee))
      continue;

    FunctionPEGInform *callee_inform = build_func_peg_inform(callee);
    update_graph_inform(callee_inform);
    callee_inform->parent = caller_inform;
    caller_inform->inform_callee_map[callee_inform] = callee_edge;
    for (unsigned i = 0; i < callee->arg_size(); i++) {
      caller_inform->links[callee_edge].
        push_back(new
                  PEG::PEGEdge(link[i]->src,
                               callee_inform->
                               arg_node_map[callee->getArg(i)],
                               PEG::PEGEDGE_TYPE::ASSIGN));
    }

    if (callee_inform->retnode) {
      caller_inform->links[callee_edge].
        push_back(new
                  PEG::PEGEdge(callee_inform->retnode,
                               callee_node,
                               PEG::PEGEDGE_TYPE::ASSIGN));
    }
  }

  for (auto edge : caller_inform->links[callee_edge]) {
    this->src_map[edge->src].push_back(edge);
    this->dst_map[edge->dst].push_back(edge);
    this->edge_map[edge] = this->edge_map[callee_edge];
  }
}

void PEGFunctionBuilder::
add_src_related_nodes(PEG::PEGVirtualNode *virtual_node,
                      std::list<PEG::PEGVirtualNode> *footprint,
                      PEG::SmallPEGNodeSet *visited_nodes,
                      bool offset) {
  if (footprint->size() > MAXLISTLENGTH)
    return;

  PEG::PEGNode* curr_node = virtual_node->pointer;
  if (curr_node->loc.type != PEG::location::GLOBAL) {
    for (auto edge : src_map[curr_node]) {
      if (edge->type == PEG::PEGEDGE_TYPE::ARG) {
        if (!this->edge_map.count(edge))
          continue;
        PEG::PEGCallNode *callee_node =
          static_cast<PEG::PEGCallNode *>(edge->dst);
        if (callee_node->indirect)
          continue;
        FunctionPEGInform *caller_inform = this->edge_map[edge];
        this->update_caller_callee_inform(caller_inform, callee_node);
      }
    }
  }

  for (auto edge : src_map[curr_node]) {
    PEG::PEGNode *dst = edge->dst;
    if (visited_nodes->contains(dst)) {
      continue;
    }

    if (dst->type == PEG::PEGNODE_TYPE::INT ||
        dst->type == PEG::PEGNODE_TYPE::STATIC)
      continue;
    // Maybe imprecise, but may work.
    // Maybe collect PEGVirtualNode more precise.
    // But fuck it, it works.
    if (edge->type != PEG::PEGEDGE_TYPE::ARG) {
      visited_nodes->insert(dst);
    }

    PEG::PEGVirtualNode new_node(dst);
    switch (edge->type) {
    case PEG::PEGEDGE_TYPE::STORE: {
      new_node.path = virtual_node->path;
      new_node.path.emplace_front(PEG::PEGVIRTUALEDGE_TYPE::LOAD, 0);
      footprint->push_back(new_node);
      break;
    }
    case PEG::PEGEDGE_TYPE::LOAD: {
      if (!virtual_node->path.empty() &&
          virtual_node->path.front().type == PEG::PEGVIRTUALEDGE_TYPE::LOAD) {
        new_node.path = virtual_node->path;
        new_node.path.pop_front();
        footprint->push_back(new_node);
      }
      break;
    }

    case PEG::PEGEDGE_TYPE::FIELD: {
      PEG::PEGFieldNode *field_node =
        static_cast<PEG::PEGFieldNode *>(dst);
      if (offset &&
          field_node->offset == 0) { // the same as bitcast
        new_node.path = virtual_node->path;
        footprint->push_back(new_node);
        break;
      }
      if (!virtual_node->path.empty() &&
          virtual_node->path.front().type == PEG::PEGVIRTUALEDGE_TYPE::FIELD &&
          virtual_node->path.front().offset == field_node->offset) {
        new_node.path = virtual_node->path;
        new_node.path.pop_front();
        footprint->push_back(new_node);
        break;
      }
      break;
    }

    case PEG::PEGEDGE_TYPE::ASSIGN: {
      new_node.path = virtual_node->path;
      footprint->push_back(new_node);
      break;
    }

    case PEG::PEGEDGE_TYPE::PHI: {
      // Whether take phi should be took into consideration
      new_node.path = virtual_node->path;
      footprint->push_back(new_node);
      break;
    }

    case PEG::PEGEDGE_TYPE::ARG:
    case PEG::PEGEDGE_TYPE::RESERVE:
    case PEG::PEGEDGE_TYPE::CMP:
    case PEG::PEGEDGE_TYPE::BINARY:
    case PEG::PEGEDGE_TYPE::COND: {
      break;
    }

    }
  }
}

void PEGFunctionBuilder::
add_dst_related_nodes(PEG::PEGVirtualNode *virtual_node,
                      std::list<PEG::PEGVirtualNode> *footprint,
                      PEG::SmallPEGNodeSet *visited_nodes,
                      bool offset) {
  if (footprint->size() > MAXLISTLENGTH)
    return;

  PEG::PEGNode* curr_node = virtual_node->pointer;
  if (curr_node->type == PEG::PEGNODE_TYPE::CALL &&
      !this->dist_peg_inform.empty()) {
    PEG::PEGCallNode *call_node =
      static_cast<PEG::PEGCallNode *>(curr_node);
    update_caller_callee_inform(this->edge_map[dst_map[curr_node].front()],
                                call_node);
  }
  for (auto edge : dst_map[curr_node]) {
    PEG::PEGNode *src = edge->src;
    if (visited_nodes->contains(src)) {
      continue;
    }
    if (src->type == PEG::PEGNODE_TYPE::INT ||
        src->type == PEG::PEGNODE_TYPE::STATIC)
      continue;
    // Maybe imprecise, but may work.
    // Maybe collect PEGVirtualNode more precise.
    // But fuck it, it works.
    if (edge->type != PEG::PEGEDGE_TYPE::ARG) {
      visited_nodes->insert(src);
    }

    PEG::PEGVirtualNode new_node(src);
    switch (edge->type) {
    case PEG::PEGEDGE_TYPE::LOAD: {
      new_node.path = virtual_node->path;
      new_node.path.emplace_front(PEG::PEGVIRTUALEDGE_TYPE::LOAD, 0);
      footprint->push_back(new_node);
      break;
     }

    case PEG::PEGEDGE_TYPE::FIELD: {
      PEG::PEGFieldNode *field_node =
        static_cast<PEG::PEGFieldNode *>(edge->dst);
      new_node.path = virtual_node->path;
      if (!offset ||
          field_node->offset != 0) { // 0 means a bitcast
        new_node.path.emplace_front(PEG::PEGVIRTUALEDGE_TYPE::FIELD,
                                    field_node->offset);
      }
      footprint->push_back(new_node);
      break;
    }

    case PEG::PEGEDGE_TYPE::ASSIGN: {
      new_node.path = virtual_node->path;
      footprint->push_back(new_node);
      break;
    }

    case PEG::PEGEDGE_TYPE::PHI: {
      // maybe not right
      new_node.path = virtual_node->path;
      footprint->push_back(new_node);
      break;
    }

    case PEG::PEGEDGE_TYPE::STORE: {
      break;
    }
    case PEG::PEGEDGE_TYPE::ARG:
    case PEG::PEGEDGE_TYPE::CMP:
    case PEG::PEGEDGE_TYPE::COND:
    case PEG::PEGEDGE_TYPE::BINARY:
    case PEG::PEGEDGE_TYPE::RESERVE: {
      break;
    }
    }
  }
}

int PEGFunctionBuilder::
identify_cv_alias(PEG::PEGNode *node) {
  PEG::SmallPEGNodeSet visited_nodes;
  std::list<PEG::PEGVirtualNode> footprint;
  for (auto inform : root_inform) {
    visited_nodes.clear();
    footprint.clear();
    PEG::PEGNode *env_node = inform->peg_node_map[node];
    visited_nodes.insert(env_node);
    footprint.emplace_back(env_node);
    while (!footprint.empty()) {
      PEG::PEGVirtualNode front = footprint.front();
      footprint.pop_front();
      if (front.path.empty()) {
        this->root_cv_alias_map[inform].insert(front.pointer);
      }
      add_src_related_nodes(&front, &footprint, &visited_nodes, true);
      add_dst_related_nodes(&front, &footprint, &visited_nodes, true);
      if (this->func_peg_inform.size() > MAXFUNCTION)
        return -1;
    }
  }
  return 0;
}

void PEGFunctionBuilder::
extend_field_nodes(PEG::SmallPEGNodeSet *set,
                   PEG::SmallPEGNodeSet *extend_set) {
  PEG::SmallPEGNodeSet pre, after;
  for (auto node : *set) {
    extend_set->insert(node);
    pre.insert(node);
  }

  while (!pre.empty()) {
    for (auto field : pre) {
      for (auto edge : this->src_map[field]) {
        if (edge->type == PEG::PEGEDGE_TYPE::FIELD) {
          extend_set->insert(edge->dst);
          after.insert(edge->dst);
        }
      }
    }
    pre = after;
    after.clear();
  }
}

void PEGFunctionBuilder::
identify_lal(FunctionPEGInform * inform, PEG::PEGNode *load_node) {
  // load after load
  PEG::SmallPEGNodeSet alias_set;
  PEG::SmallPEGNodeSet fields_set;
  PEG::SmallPEGNodeSet visited_nodes;
  std::list<PEG::PEGVirtualNode> footprint;
  visited_nodes.clear();
  footprint.clear();
  visited_nodes.insert(load_node);
  footprint.emplace_back(load_node);
  while (!footprint.empty()) {
    PEG::PEGVirtualNode front = footprint.front();
    footprint.pop_front();
    if(front.path.empty()) {
      alias_set.insert(front.pointer);
    }
    add_src_related_nodes(&front, &footprint, &visited_nodes, true);
    add_dst_related_nodes(&front, &footprint, &visited_nodes, true);
  }
  for (auto node : alias_set) {
    for (auto edge : src_map[node]) {
      if (edge->type == PEG::PEGEDGE_TYPE::FIELD) {
        fields_set.insert(edge->dst);
      }
    }
    fields_set.insert(node);
  }

  for (auto node : fields_set) {
    for (auto edge : src_map[node]) {
      if (edge->type == PEG::PEGEDGE_TYPE::LOAD) {
        this->root_cv_uses_map[inform].insert(edge);
        this->cv_uses.insert(edge);
      }
    }
  }
}

void PEGFunctionBuilder::identify_cv_use(bool add) {
  PEG::SmallPEGNodeSet fields;
  for (auto alias_map : this->root_cv_alias_map) {
    fields.clear();
    FunctionPEGInform *inform = alias_map.first;
    PEG::SmallPEGNodeSet *cv_alias = &alias_map.second;
    extend_field_nodes(cv_alias, &fields);
    for (auto field : fields) {
      for (auto edge : src_map[field]) {
        switch (edge->type) {
        case PEG::PEGEDGE_TYPE::LOAD: {
#if CV_INIT
          identify_lal(inform, edge->dst);
#else
          this->root_cv_uses_map[inform].insert(edge);
          this->cv_uses.insert(edge);
#endif
          break;
        }

        case PEG::PEGEDGE_TYPE::ARG: {
          PEG::PEGCallNode *call_node =
            static_cast<PEG::PEGCallNode *>(edge->dst);
          if (call_node->callee == "list_add_tail") {
            this->root_cv_uses_map[inform].insert(edge);
            this->cv_uses.insert(edge);
          }
#if RELEASE_COLLECT
          if (ctx->release_funcs.count(call_node->callee.str())) {
            this->root_cv_uses_map[inform].insert(edge);
            this->cv_uses.insert(edge);
          }
#endif
          break;
        }
        case PEG::PEGEDGE_TYPE::CMP:
        case PEG::PEGEDGE_TYPE::COND: {
          if (add) {
            this->root_cv_uses_map[inform].insert(edge);
            this->cv_uses.insert(edge);
          }
          break;
        }
        case PEG::PEGEDGE_TYPE::PHI:
        case PEG::PEGEDGE_TYPE::ASSIGN:
        case PEG::PEGEDGE_TYPE::FIELD:
        case PEG::PEGEDGE_TYPE::RESERVE:
        case PEG::PEGEDGE_TYPE::BINARY:
        case PEG::PEGEDGE_TYPE::STORE: {
          break;
        }
        }
      }

      for (auto edge : dst_map[field]) {
        switch (edge->type) {
        case PEG::PEGEDGE_TYPE::STORE: {
          this->root_cv_uses_map[inform].insert(edge);
          this->cv_uses.insert(edge);
          break;
        }
        case PEG::PEGEDGE_TYPE::ARG:
        case PEG::PEGEDGE_TYPE::PHI:
        case PEG::PEGEDGE_TYPE::ASSIGN:
        case PEG::PEGEDGE_TYPE::FIELD:
        case PEG::PEGEDGE_TYPE::CMP:
        case PEG::PEGEDGE_TYPE::COND:
        case PEG::PEGEDGE_TYPE::RESERVE:
        case PEG::PEGEDGE_TYPE::BINARY:
        case PEG::PEGEDGE_TYPE::LOAD: {
          break;
        }
        }
      }
    }
  }
}

void PEGFunctionBuilder::
build_one_inform(FunctionPEGInform *inform) {
  if (dist_peg_inform.count(inform))
    return;

  dist_peg_inform.insert(inform);
  for (auto& bb : *inform->func) {
    for (auto edge : inform->bb_edges_map[&bb]) {
      src_map[edge->src].push_back(edge);
      dst_map[edge->dst].push_back(edge);
    }
  }

  for (auto link : inform->links) {
    for (auto edge : link.second) {
      src_map[edge->src].push_back(edge);
      dst_map[edge->dst].push_back(edge);
    }
  }
  for (auto map : inform->inform_callee_map) {
    build_one_inform(map.first);
  }
}

void PEGFunctionBuilder::rebuild_graph_inform() {
  src_map.clear();
  dst_map.clear();
  for (auto& path : this->dist_env) {
    for (auto inform : path) {
      if (this->dist_peg_inform.count(inform))
        continue;

      if (root_inform.contains(inform)) {
        build_one_inform(inform);
        continue;
      }

      this->dist_peg_inform.insert(inform);
      for (auto& bb : *inform->func) {
        for (auto edge : inform->bb_edges_map[&bb]) {
          src_map[edge->src].push_back(edge);
          dst_map[edge->dst].push_back(edge);
        }
      }
      for (auto link : inform->dist_links) {
        for (auto edge : inform->links[link]) {
          src_map[edge->src].push_back(edge);
          dst_map[edge->dst].push_back(edge);
        }
      }
    }
  }
}

bool PEGFunctionBuilder::edge_lt(PEG::PEGEdge *edge1, PEG::PEGEdge *edge2,
                                 FunctionPEGInform *inform) {
  if (edge1 == nullptr)
    return true;

  if (edge1 == edge2)
    return false;

  BasicBlock* start_bb = inform->edge_bb_map[edge1];
  BasicBlock* end_bb = inform->edge_bb_map[edge2];
  if (start_bb == end_bb) {
    for (auto edge : inform->bb_edges_map[start_bb]) {
      if (edge == edge1)
        return true;
      if (edge == edge2)
        return false;
    }
  } else {
    return Tools::from_bba_to_bbb(start_bb, end_bb);
  }

  OP << KRED << "Impossible for edge_lt\n" << KNRM;
  return false;
}

void PEGFunctionBuilder::build_call_path(FunctionPEGInform *inform,
                                         CallPath *path) {
  path->clear();
  FunctionPEGInform *tmp = inform;
  while (tmp != nullptr) {
    path->push_back(tmp);
    tmp = tmp->parent;
  }
}

void PEGFunctionBuilder::
add_dist_env(CallPath *path, unsigned stop) {
  for (unsigned i = 1; i <= stop; i++) {
    (*path)[i]->dist_links.insert((*path)[i]->
                                  inform_callee_map[(*path)[i - 1]]);
  }
}

bool PEGFunctionBuilder::
possible_judge(CallPath *path1, unsigned merge1,
               CallPath *path2, unsigned merge2,
               PEG::SmallPEGEdgeSet *cv_uses) {
  FunctionPEGInform *caller_inform = (*path1)[merge1];
  PEG::PEGEdge *edge1 =
    caller_inform->inform_callee_map[(*path1)[merge1 - 1]];
  if (merge2 == 0) {
    for (auto use : *cv_uses) {
      if (this->edge_map[use] == (*path2)[merge2]) {
        if (edge_lt(edge1, use, caller_inform)) {
          return true;
        }
      }
    }
    return false;
  }

  PEG::PEGEdge *edge2 =
    caller_inform->inform_callee_map[(*path2)[merge2 - 1]];
  return edge_lt(edge1, edge2, caller_inform);
}

void PEGFunctionBuilder::
merge_two_path(CallPath *path1, unsigned merge1,
               CallPath *path2, unsigned merge2,
               CallPath *merge_path) {
  merge_path->clear();

  for (unsigned i = 0; i < merge1; i++) {
    merge_path->push_back((*path1)[i]);
  }
  for (int i = merge2; i >= 0; i--) {
    merge_path->push_back((*path2)[i]);
  }
}

void PEGFunctionBuilder::
collect_paths(CallPath *origin_path, CallPath *use_path,
              PEG::SmallPEGEdgeSet *cv_uses) {
  CallPath merge_path;
  for (unsigned i = 1; i < origin_path->size(); i++) {
    for (unsigned j = 0; j < use_path->size(); j++) {
      if ((*origin_path)[i] == (*use_path)[j]) {
        if (possible_judge(origin_path, i, use_path, j, cv_uses)) {
          add_dist_env(origin_path, i);
          add_dist_env(use_path, j);
          merge_two_path(origin_path, i, use_path, j,
                         &merge_path);
          this->dist_env.insert(merge_path);
        }
        return;
      }
    }
  }
}

void PEGFunctionBuilder::
collect_call_path(FunctionPEGInform *inform,
                  PEG::SmallPEGEdgeSet *cv_uses) {
  CallPath origin_path;
  this->build_call_path(inform, &origin_path);

  FunctionPEGInformSet use_informs;
  for (auto edge : *cv_uses) {
    use_informs.insert(this->edge_map[edge]);
  }
  use_informs.erase(inform);

  CallPath use_path;
  for (auto use_inform : use_informs) {
    this->build_call_path(use_inform, &use_path);
    if (std::find(use_path.begin(), use_path.end(), inform)
        != use_path.end())
      continue;
    collect_paths(&origin_path, &use_path, cv_uses);
  }
}

int PEGFunctionBuilder::build_dist_env() {
  for (auto map : this->root_cv_uses_map) {
    FunctionPEGInform *inform = map.first;
    PEG::SmallPEGEdgeSet *cv_uses = &map.second;
    collect_call_path(inform, cv_uses);
  }

  rebuild_graph_inform();
  return this->dist_peg_inform.size();
}

void PEGFunctionBuilder::
add_node_to_cache(PEG::PEGVirtualNode *front,
                  std::list<PEG::PEGVirtualNode> *visited_footprint) {
  for (auto foot : *visited_footprint) {
    if (front->path == foot.path) {
      dist_cache[foot.pointer].insert(front->pointer);
      return;
    }
  }
}

void PEGFunctionBuilder::
identify_dist_without_cache(PEG::PEGNode *dist) {
  PEG::SmallPEGNodeSet visited_nodes;
  std::list<PEG::PEGVirtualNode> footprint;
  for (auto inform : root_inform) {
    PEG::PEGNode *node = inform->peg_node_map[dist];
    visited_nodes.insert(node);
    footprint.emplace_back(node);
  }

  while (!footprint.empty()) {
    PEG::PEGVirtualNode front = footprint.front();
    footprint.pop_front();
    if (front.path.empty()) {
      this->dist_alias.insert(front.pointer);
    }
    add_src_related_nodes(&front, &footprint, &visited_nodes, false);
    add_dst_related_nodes(&front, &footprint, &visited_nodes, false);
  }
}

void PEGFunctionBuilder::identify_cmp_ret(PEG::PEGNode *cmp_node) {
  std::list<PEG::PEGNode *> nodes;
  std::set<PEG::PEGNode *> nodes_set;
  nodes.push_back(cmp_node);
  nodes_set.insert(cmp_node);
  while (!nodes.empty()) {
    PEG::PEGNode *node = nodes.front();
    nodes.pop_front();
    for (auto edge : src_map[node]) {
      switch (edge->type) {
      case PEG::PEGEDGE_TYPE::CMP: {
        if (!nodes_set.count(edge->dst)) {
          nodes_set.insert(edge->dst);
          nodes.push_back(edge->dst);
        }
        break;
      }
      case PEG::PEGEDGE_TYPE::COND: {
        this->dist_uses.insert(edge);
        break;
      }

      case PEG::PEGEDGE_TYPE::ASSIGN: {
        if (edge->dst->type == PEG::PEGNODE_TYPE::RET) {
          PEG::PEGNode *ret_node = edge->dst;
          for (auto call_edge : src_map[ret_node]) {
            if (call_edge->type == PEG::PEGEDGE_TYPE::ASSIGN &&
                call_edge->dst->type == PEG::PEGNODE_TYPE::CALL) {
              if (!nodes_set.count(call_edge->dst)) {
                nodes_set.insert(call_edge->dst);
                nodes.push_back(call_edge->dst);
              }
            }
          }
        }
        if (edge->dst->type == PEG::PEGNODE_TYPE::SELECT)
          if (!nodes_set.count(edge->dst)) {
            nodes_set.insert(edge->dst);
            nodes.push_back(edge->dst);
          }
        break;
      }

      case PEG::PEGEDGE_TYPE::PHI: {
        if (!nodes_set.count(edge->dst)) {
          nodes_set.insert(edge->dst);
          nodes.push_back(edge->dst);
        }
        break;
      }

      case PEG::PEGEDGE_TYPE::BINARY: {
        this->dist_uses.insert(edge);
        break;
      }
      default:
        break;
      }
    }
  }
}

void PEGFunctionBuilder::identify_dist_use() {
  std::list<PEG::PEGNode *> nodes;
  for (auto node : this->dist_alias) {
    nodes.push_back(node);
  }
  while (!nodes.empty()) {
    PEG::PEGNode *node = nodes.front();
    nodes.pop_front();
    for (auto edge : src_map[node]) {
      switch (edge->type) {
      case PEG::PEGEDGE_TYPE::CMP: {
        identify_cmp_ret(edge->dst);
        break;
      }

      case PEG::PEGEDGE_TYPE::COND: {
        this->dist_uses.insert(edge);
        break;
      }

      case PEG::PEGEDGE_TYPE::ARG: {
        PEG::PEGCallNode *call_node =
          static_cast<PEG::PEGCallNode *>(edge->dst);
        if (call_node->callee.contains("IS_ERR") ||
            call_node->callee.contains("test_ptr")) {
          //Those are checking related function
          nodes.push_back(edge->dst);
        }

        break;
      }

      case PEG::PEGEDGE_TYPE::ASSIGN: {
        break;
      }

      case PEG::PEGEDGE_TYPE::LOAD: {
        nodes.push_back(edge->dst);
        break;
      }

      case PEG::PEGEDGE_TYPE::PHI:
      case PEG::PEGEDGE_TYPE::FIELD:
      case PEG::PEGEDGE_TYPE::BINARY:
      case PEG::PEGEDGE_TYPE::RESERVE:
      case PEG::PEGEDGE_TYPE::STORE: {
        break;
      }
      }
    }
  }
}

bool PEGFunctionBuilder::judge() {
  if (dist_uses.empty()) {
    OP << KRED << "ERROR: situation 1: "
       << "no distinguisher at all\n" << KNRM;
    for (auto path : this->dist_env) {
      for (auto inform : path) {
        OP << inform->func->getName() << "-->";
      }
      OP << "end\n";
    }
    return false;
  }

  FunctionPEGInformSet dist_inform;
  for (auto edge : this->dist_uses) {
    dist_inform.insert(this->edge_map[edge]);
  }
  bool ret_flag = true;

  for (auto path : this->dist_env) {
    bool flag = false;
    for (unsigned i = 1; i < path.size(); i++) { // no need to check path[0]
      if (dist_inform.contains(path[i])) {
        flag = true;
        break;
      }
    }
    if (ret_flag)
      ret_flag = flag;

    if (!flag) {
      for (auto inform : path) {
        OP << inform->func->getName() << "-->";
      }
      OP << "end\n";
    }
  }

  return ret_flag;
}
