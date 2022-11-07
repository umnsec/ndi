#include "PEGNode.hh"

namespace PEG {
  llvm::raw_ostream &operator<<(llvm::raw_ostream &out, PEGNode &node) {
    out << KYEL << node.index << ": " << KNRM;
    switch (node.type) {
    case (PEGNODE_TYPE::STATIC) :
      out << "STATIC\n";
      break;

    case (PEGNODE_TYPE::POINTER) :
      out << "Pointer\n";
      break;

    case (PEGNODE_TYPE::OBJECT) :
      out << "Object\n";
      break;

    case (PEGNODE_TYPE::FIELD): {
      PEGFieldNode *field_node = static_cast<PEGFieldNode *>(&node);
      out << "Field, ";
      if (field_node->field_type == PEGNODE_TYPE::POINTER)
        out << "Pointer, ";
      else
        out << "Object, ";
      out << "Offset: ";
      out << KGRN << field_node->offset << "\n" << KNRM;
      out << "Pointer is:\n";
      out << KMAG << "--------------------------------\n" << KNRM;
      out << *field_node->pointer;
      out << KMAG << "--------------------------------\n" << KNRM;
      break;
    }

    case (PEGNODE_TYPE::CALL): {
      PEGCallNode *call_node = static_cast<PEGCallNode *>(&node);
      out << "Call\n";
      out << KGRN << call_node->callee << "\n" << KNRM;;
      break;
    }

    case (PEGNODE_TYPE::CMP): {
      out << "Cmp\n";
      break;
    }

    case (PEGNODE_TYPE::PHI): {
      out << "Phi\n";
      break;
    }

    case (PEGNODE_TYPE::COND): {
      out << "Cond\n";
      break;
    }

    case (PEGNODE_TYPE::SELECT): {
      out << "Select\n";
      break;
    }


    case (PEGNODE_TYPE::INT): {
      PEGIntNode *int_node = static_cast<PEGIntNode *>(&node);
      out << "Int: " << KGRN << int_node->int_val << "\n" << KNRM;
      break;
    }

    case (PEGNODE_TYPE::RET):
      out << "Ret\n";
      break;
    }

    if (node.value)
      out << KMAG << *node.value << KNRM << "\n";

    switch (node.loc.type) {
    case (location::TYPE::LOCAL): {
      if (node.loc.loc_bb &&
          node.loc.loc_func) {
        node.loc.loc_bb->printAsOperand(out, false);
        out << " | " << node.loc.loc_func->getName() << "\n";
      }
      else {
         out << "NULL " << " | " << "NULL " << "\n";
      }
      break;
    }

    case (location::TYPE::GLOBAL): {
      out << "Global\n";
      break;
    }

    case (location::TYPE::ARGUMENT): {
      out << "Argument | ";
      if (node.loc.loc_func)
        out << node.loc.loc_func->getName() << "\n";
      else
        out << "NULL\n";
      break;
    }

    }
    return out;
  }

  llvm::raw_ostream &operator<<(llvm::raw_ostream &out, PEGEdge &edge) {
    out << *edge.src;
    out << KBLU;
    switch (edge.type) {
    case (PEGEDGE_TYPE::STORE):
      out << "Store\n";
      break;

    case (PEGEDGE_TYPE::LOAD):
      out << "Load\n";
      break;

    case (PEGEDGE_TYPE::FIELD):
      out << "Field\n";
      break;

    case (PEGEDGE_TYPE::ASSIGN):
      out << "Assign\n";
      break;

    case (PEGEDGE_TYPE::CMP):
      out << "Cmp\n";
      break;

    case (PEGEDGE_TYPE::COND):
      out << "Cond\n";
      break;

    case (PEGEDGE_TYPE::PHI):
      out << "Phi\n";
      break;

    case (PEGEDGE_TYPE::ARG):
      out << "Arg\n";
      break;

    case (PEGEDGE_TYPE::BINARY):
      out << "Binary\n";
      break;

    case (PEGEDGE_TYPE::RESERVE):
      out << "Reserve\n";
      break;
    }
    out << KNRM << *edge.dst;
    return out;
  }

  llvm::raw_ostream &operator<<(llvm::raw_ostream &out, PEGVirtualNode &node) {
    out << *node.pointer;
    out << "-----------------------\n";
      for (auto &edge : node.path) {
        if (edge.type == PEG::PEGVIRTUALEDGE_TYPE::LOAD)
          out << KYEL << "Load" << KNRM << "\n";
        else
          out << KYEL << "offset: " << KNRM << edge.offset << "\n";
      }
      return out;
  }

  void set_location(Value *v, struct location *loc) {
    if (isa<Constant>(v))
      loc->type = location::TYPE::GLOBAL;
    else {
      if (isa<Argument>(v)) {
        loc->type = location::TYPE::ARGUMENT;
        loc->loc_func = cast<Argument>(v)->getParent();
      } else {
        loc->type = location::TYPE::LOCAL;
        loc->loc_bb = cast<Instruction>(v)->getParent();
        if (loc->loc_bb) // else it will be <badref>, no basicblock
          loc->loc_func = loc->loc_bb->getParent();
      }
    }
  }

  PEGNodeList pegnode_list;
  std::map<int64_t, PEGIntNode *> int_pegintnode_map;
  llvm::DenseMap<Constant *, PEGNode *> constant_node_map;

  PEGNode *create_pegnode(Value *v) {
    int num = pegnode_list.size();
    PEGNode *node = nullptr;
    if (v->getType()->isPointerTy()) {
      node = new PEGNode(PEGNODE_TYPE::POINTER, num);
    }
    else {
      node = new PEGNode(PEGNODE_TYPE::OBJECT, num);
    }
    set_location(v, &node->loc);
    node->value = v;
    pegnode_list.push_back(node);
    return node;
  }

  void PEGFieldNode::update(GetElementPtrInst *gep_ins,
                            struct location *loc,
                            PEGNode *pointer) {
    // no update offset -> update in mapping
    this->loc = *loc;
    this->pointer = pointer;
    if (gep_ins->getType()->isPointerTy()) {
      this->field_type = PEGNODE_TYPE::POINTER;
    } else {
      this->field_type = PEGNODE_TYPE::OBJECT;
    }
  }

  void PEGCmpNode::update(PEGNode *lhs,
                          llvm::CmpInst::Predicate op,
                          PEGNode *rhs) {
    this->lhs = lhs;
    this->op = op;
    this->rhs = rhs;
  }

  void PEGCallNode::update(CallBase *call_ins,
                           PEGNodeList *args,
                           PEGEdge *call_edge) {
    this->indirect = call_ins->isIndirectCall();
    if (call_ins->getCalledFunction()) {
      this->callee = call_ins->getCalledFunction()->getName();
    }
    else {
      this->callee = "";
    }
    if (args)
      this->args = *args;
    this->call_edge = call_edge;
  }

  void PEGPhiNode::update(PHINode *phi_ins, PEGNodeList *args) {
    for (unsigned i = 0; i < phi_ins->getNumIncomingValues(); i++) {
      this->incomings[phi_ins->getIncomingBlock(i)] = (*args)[i];
    }
    this->curr_bb = phi_ins->getParent();
  }

  void PEGSelectNode::update(PEGNode *cond,
                             PEGNode *true_node,
                             PEGNode *false_node) {
    this->cond = cond;
    this->true_node = true_node;
    this->false_node = false_node;
  }

  void PEGBrNode::update(BranchInst *br_ins,
                         PEGNode *cond_node) {
    this->true_bb = br_ins->getSuccessor(0);
    this->false_bb = br_ins->getSuccessor(1);
    this->cond_node = cond_node;
  }

  PEGIntNode *get_or_create_pegintnode(ConstantInt *v) {
    int64_t int_val;
    if (v->getBitWidth() <= 64) {
      int_val = v->getSExtValue();
    }
    else
      int_val = v->getLimitedValue();

    if (int_pegintnode_map.count(int_val))
      return int_pegintnode_map[int_val];

    int num = int_pegintnode_map.size();
    PEGIntNode *node = new PEGIntNode(int_val, num);
    int_pegintnode_map[int_val] = node;
    return node;
  }


  PEGNode *get_or_create_pegconstnode(Constant *v) {
    if (constant_node_map.count(v)) {
      return constant_node_map[v];
    }

    int num = constant_node_map.size() + 1; // 0 for null
    PEGNode *node = new PEGNode(PEGNODE_TYPE::STATIC, num);
    constant_node_map[v] = node;
    return node;
  }

  void init_peg_env() {
    assert(pegnode_list.empty());
    // insert null node
    PEG::PEGNode *null_node = new PEGNode(PEGNODE_TYPE::STATIC, 0);
    null_node->loc.type = location::GLOBAL;
    pegnode_list.push_back(null_node);
    // insert int 0
    PEGIntNode *zero_node = new PEGIntNode(0, 0);
    int_pegintnode_map[0] = zero_node;
  }

  bool virtualnodesset_contains(PEGVirtualNodesSet *set,
                                PEGVirtualNode *v_node) {
    for (auto node : *set) {
      if (*node == *v_node) {
        return true;
      }
    }
    return false;
  }

  PEGNode *copy_node(PEGNode *node) {
    if (node->type == PEG::PEGNODE_TYPE::STATIC ||
        node->type == PEG::PEGNODE_TYPE::INT)
      return node;

    if (node->loc.type == PEG::location::GLOBAL)
      return node;

    if (node->type == PEG::PEGNODE_TYPE::POINTER ||
        node->type == PEG::PEGNODE_TYPE::OBJECT ||
        node->type == PEG::PEGNODE_TYPE::RET) {
      PEGNode *ret_node = new PEGNode();
      *ret_node = *node;
      return ret_node;
    }

    if (node->type == PEG::PEGNODE_TYPE::FIELD) {
      PEGFieldNode *field_node = static_cast<PEGFieldNode *>(node);
      PEGFieldNode *ret_node = new PEGFieldNode();
      *ret_node = *field_node;
      return ret_node;
    }

    if (node->type == PEG::PEGNODE_TYPE::CMP) {
      PEGCmpNode *cmp_node = static_cast<PEGCmpNode *>(node);
      PEGCmpNode *ret_node = new PEGCmpNode();
      *ret_node = *cmp_node;
      return ret_node;
    }

    if (node->type == PEG::PEGNODE_TYPE::CALL) {
      PEGCallNode *call_node = static_cast<PEGCallNode *>(node);
      PEGCallNode *ret_node = new PEGCallNode();
      *ret_node = *call_node;
      return ret_node;
    }

    if (node->type == PEG::PEGNODE_TYPE::PHI) {
      PEGPhiNode *phi_node = static_cast<PEGPhiNode *>(node);
      PEGPhiNode *ret_node = new PEGPhiNode();
      *ret_node = *phi_node;
      return ret_node;
    }

    if (node->type == PEG::PEGNODE_TYPE::COND) {
      PEGBrNode *cond_node = static_cast<PEGBrNode *>(node);
      PEGBrNode *ret_node = new PEGBrNode();
      *ret_node = *cond_node;
      return ret_node;
    }

    if (node->type == PEG::PEGNODE_TYPE::SELECT) {
      PEGSelectNode *select_node = static_cast<PEGSelectNode *>(node);
      PEGSelectNode *ret_node = new PEGSelectNode();
      *ret_node = *select_node;
      return ret_node;
    }


    OP << KRED << "we should not reach here: copy_node in PEGNode\n"
       << KNRM;
    return nullptr;
  }
}
