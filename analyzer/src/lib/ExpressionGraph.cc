#include "llvm/ADT/APInt.h"
#include "llvm/IR/InstIterator.h"

#include "ExpressionGraph.hh"

Value *ExpressionGraphPass::create_constantexpr(Value *v) {
  if (ConstantExpr *v_expr = dyn_cast<ConstantExpr>(v)) {
    Instruction *ins = v_expr->getAsInstruction();
    constantexpr_ins_map[v_expr] = ins;
    if (!create_instruction_node(ins)) {
      OP << KRED << "err in creating node for CONSTANT ins " << *ins
         << "\n" << KNRM;
      return nullptr;
    }
    if (value_pegnode_map.count(ins))
      value_pegnode_map[v_expr] = value_pegnode_map[ins];
  }
  return v;
}

Value *ExpressionGraphPass::
translate_constantexpr(Value *v,
                       PEG::PEGEdgeList *edge_list) {
  if (ConstantExpr *v_expr = dyn_cast<ConstantExpr>(v)) {
    Instruction *ins = constantexpr_ins_map[v_expr];
    if (translate_instruction(ins, edge_list)) {
      value_pegnode_map[v_expr] = value_pegnode_map[ins];
      return v;
    }
    else {
      OP << KRED << "err in adding CONSTANT ins " << *ins << "\n" << KNRM;
      return nullptr;
    }
  }

  return v;
}

PEG::PEGNode *ExpressionGraphPass::get_pegnode(Value *v) {
  if (v == NULL || isa<ConstantPointerNull>(v)) {
    return PEG::pegnode_list[0];
  }

  if (isa<UndefValue>(v)) {
    return PEG::pegnode_list[0]; // return anything is OK.
  }

  if (value_pegnode_map.count(v))
    return value_pegnode_map[v];

  if (ConstantInt *cons_int = dyn_cast<ConstantInt>(v)) {
    return PEG::get_or_create_pegintnode(cons_int);
  }

  if (Constant *cons = dyn_cast<Constant>(v)) {
    return PEG::get_or_create_pegconstnode(cons);
  }
  
  return nullptr;
}

bool ExpressionGraphPass::create_instruction_node(Instruction *ins) {
   if (value_pegnode_map.count(ins)) {
    OP << KRED << *ins << " redefine ins and node\n" << KNRM;
    return false;
  }

  if (AllocaInst *alloca_ins = dyn_cast<AllocaInst>(ins)) {
    PEG::PEGNode *node = PEG::create_pegnode(alloca_ins);
    value_pegnode_map[alloca_ins] = node;
    return true;
  }

  if (StoreInst *store_ins = dyn_cast<StoreInst>(ins)) {
    create_constantexpr(store_ins->getValueOperand());
    create_constantexpr(store_ins->getPointerOperand());
    return true;
  }

  if (LoadInst *load_ins = dyn_cast<LoadInst>(ins)) {
    // WARNING: What should we do if we have several loads on same variable?
    // Now we will create different load node, and we may merge them after.
    // No need to merge: load represnts different value:
    // a = 1; load a; a = 2; load a;
    create_constantexpr(load_ins->getPointerOperand());
    PEG::PEGNode *load_node = PEG::create_pegnode(load_ins);
    value_pegnode_map[load_ins] = load_node;
    return true;
  }

  if (AtomicRMWInst *rmw_ins = dyn_cast<AtomicRMWInst>(ins)) {
    // handle this like load
    create_constantexpr(rmw_ins->getPointerOperand());
    PEG::PEGNode *rmw_node = PEG::create_pegnode(rmw_ins);
    value_pegnode_map[rmw_ins] = rmw_node;
    return true;
  }

  if (GetElementPtrInst *gep_ins = dyn_cast<GetElementPtrInst>(ins)) {
    create_constantexpr(gep_ins->getPointerOperand());
    return true;
  }

  if (CastInst *cast_ins = dyn_cast<CastInst>(ins)) {
    // handle later
    create_constantexpr(cast_ins->getOperand(0));
    return true;
  }

  if (BlockAddress *ba_ins = dyn_cast<BlockAddress>(ins)) {
    PEG::PEGNode *node = PEG::create_pegnode(ba_ins);
    value_pegnode_map[ba_ins] = node;
    return true;
  }

  if (SelectInst *select_ins = dyn_cast<SelectInst>(ins)) {
    create_constantexpr(select_ins->getTrueValue());
    create_constantexpr(select_ins->getFalseValue());
    PEG::PEGSelectNode *node =
      PEG::create_pegnode<PEG::PEGSelectNode>(select_ins);
    value_pegnode_map[select_ins] = node;
    return true;
  }

  if (ExtractValueInst *ev_ins = dyn_cast<ExtractValueInst>(ins)) {
    create_constantexpr(ev_ins->getAggregateOperand());
    return true;
  }

  if (ExtractElementInst *ee_ins = dyn_cast<ExtractElementInst>(ins)) {
    create_constantexpr(ee_ins->getVectorOperand());
    return true;
  }

  if (InsertElementInst *ie_ins = dyn_cast<InsertElementInst>(ins)) {
    create_constantexpr(ie_ins->getOperand(0));
    create_constantexpr(ie_ins->getOperand(1));
    PEG::PEGNode *ie_node = PEG::create_pegnode(ie_ins);
    value_pegnode_map[ie_ins] = ie_node;
    return true;
  }

  if (InsertValueInst *iv_ins = dyn_cast<InsertValueInst>(ins)) {
    create_constantexpr(iv_ins->getAggregateOperand());
    create_constantexpr(iv_ins->getInsertedValueOperand ());
    PEG::PEGNode *iv_node = PEG::create_pegnode(iv_ins);
    value_pegnode_map[iv_ins] = iv_node;
    return true;
  }

  if (CmpInst *cmp_ins = dyn_cast<CmpInst>(ins)) {
    create_constantexpr(cmp_ins->getOperand(0));
    create_constantexpr(cmp_ins->getOperand(1));
    PEG::PEGCmpNode *cmp_node = PEG::create_pegnode<PEG::PEGCmpNode>(cmp_ins);
    value_pegnode_map[cmp_ins] = cmp_node;
    return true;
  }

  if (BinaryOperator *binary_ins = dyn_cast<BinaryOperator>(ins)) {
    // we simply create a pegnode for binary_ins
    // anyway it is too rare and almost has nothing to do with this task
    PEG::PEGNode *node = PEG::create_pegnode(binary_ins);
    value_pegnode_map[binary_ins] = node;
    return true;
  }

  if (UnaryOperator *unary_ins = dyn_cast<UnaryOperator>(ins)) {
    // the same as binary
    PEG::PEGNode *node = PEG::create_pegnode(unary_ins);
    value_pegnode_map[unary_ins] = node;
    return true;
  }

  // we do not handle them
  if (CallBrInst *callbr_ins = dyn_cast<CallBrInst>(ins)) {
    // must handled before callbase
    return true;
  }

  if (CallBase *call_ins = dyn_cast<CallBase>(ins)) {
    for (auto &origin_arg : call_ins->args()) {
      create_constantexpr(origin_arg);
    }
    PEG::PEGCallNode *call_node = PEG::create_pegnode<PEG::PEGCallNode>(call_ins);
    value_pegnode_map[call_ins] = call_node;
    return true;
  }

  if (PHINode *phi_ins = dyn_cast<PHINode>(ins)) {
    for (unsigned i = 0; i < phi_ins->getNumIncomingValues(); i++) {
      create_constantexpr(phi_ins->getIncomingValue(i));
    }
    PEG::PEGPhiNode *phi_node = PEG::create_pegnode<PEG::PEGPhiNode>(phi_ins);
    value_pegnode_map[phi_ins] = phi_node;
    return true;
  }

  if (ShuffleVectorInst *sv_ins = dyn_cast<ShuffleVectorInst>(ins)) {
    create_constantexpr(sv_ins->getOperand(0));
    create_constantexpr(sv_ins->getOperand(1));
    PEG::PEGNode *sv_node = PEG::create_pegnode(sv_ins);
    value_pegnode_map[sv_ins] = sv_node;
    return true;
  }

  if (BranchInst *br_ins = dyn_cast<BranchInst>(ins)) {
    if (br_ins->isUnconditional())
      return true;
    PEG::PEGBrNode *br_node = PEG::create_pegnode<PEG::PEGBrNode>(br_ins);
    value_pegnode_map[br_ins] = br_node;
    return true;
  }

  if (FreezeInst *freeze_ins = dyn_cast<FreezeInst>(ins)) {
    return true;
  }

  // we do not handle error handling system... for now
  if (LandingPadInst *lp_ins = dyn_cast<LandingPadInst>(ins)) {
    PEG::PEGNode *node = PEG::create_pegnode(lp_ins);
    value_pegnode_map[lp_ins] = node;
    return true;
  }

  if (ResumeInst *lp_ins = dyn_cast<ResumeInst>(ins)) {
    return true;
  }

  if (SwitchInst *switch_ins = dyn_cast<SwitchInst>(ins)) {
    // add siwtch situation
    PEG::PEGNode *switch_node = PEG::create_pegnode(switch_ins);
    value_pegnode_map[switch_ins] = switch_node;
    return true;
  }

  if (UnreachableInst *un_ins = dyn_cast<UnreachableInst>(ins)) {
    return true;
  }

  if (FenceInst *fence_ins = dyn_cast<FenceInst>(ins)) {
    return true;
  }

  if (ReturnInst *ret_ins = dyn_cast<ReturnInst>(ins)) {
    if (!ret_ins->getReturnValue()) //return void. simply return true;
      return true;
    create_constantexpr(ret_ins->getReturnValue());
    PEG::PEGNode *ret_node = PEG::create_pegnode(ret_ins);
    ret_node->type = PEG::PEGNODE_TYPE::RET;
    value_pegnode_map[ret_ins] = ret_node;
    Ctx->func_retnode_map[ret_ins->getFunction()] = ret_node;
    return true;
  }

  return false;
}

bool ExpressionGraphPass::map_instruction(Instruction *ins) {
  if (value_pegnode_map.count(ins))
    return true;

  if (GetElementPtrInst *gep_ins = dyn_cast<GetElementPtrInst>(ins)) {
    PEG::PEGNode *pointer_node =
      get_pegnode(gep_ins->getPointerOperand());
    if (pointer_node == nullptr) {
      return false;
    }
    if (!gep_ins->hasAllConstantIndices()) {
      value_pegnode_map[gep_ins] = pointer_node;
      pointer_node->partly = true;
    } else {
      llvm::APInt offset(64, 0);
      gep_ins->accumulateConstantOffset(*datalayout, offset);
      int64_t offset_int = offset.getSExtValue();
      if (node_fields_map.count(pointer_node)) {
        for (auto field : node_fields_map[pointer_node]) {
          if (field->offset == offset_int) {
            value_pegnode_map[gep_ins] = field;
            return true;
          }
        }
      }

      PEG::PEGFieldNode *gep_node =
        PEG::create_pegnode<PEG::PEGFieldNode>(gep_ins);
      value_pegnode_map[gep_ins] = gep_node;
      gep_node->offset = offset_int;
      node_fields_map[pointer_node].insert(gep_node);
    }
    return true;
  }

  if (CastInst *cast_ins = dyn_cast<CastInst>(ins)) {
    PEG::PEGNode *origin_node = get_pegnode(cast_ins->getOperand(0));
    if (origin_node == nullptr) {
      return false;
    }
    value_pegnode_map[cast_ins] = origin_node;
    return true;
  }

  if (ExtractValueInst *ev_ins = dyn_cast<ExtractValueInst>(ins)) {
    PEG::PEGNode *agg_node = get_pegnode(ev_ins->getAggregateOperand());
    if (agg_node == nullptr) {
      return false;
     }
     value_pegnode_map[ev_ins] = agg_node;
     return true;
  }

  if (ExtractElementInst *ee_ins = dyn_cast<ExtractElementInst>(ins)) {
    PEG::PEGNode *vector_node = get_pegnode(ee_ins->getVectorOperand());
    if (vector_node == nullptr) {
      return false;
     }
     value_pegnode_map[ee_ins] = vector_node;
     return true;
  }

  if (FreezeInst *freeze_ins = dyn_cast<FreezeInst>(ins)) {
    PEG::PEGNode *freeze_node = get_pegnode(freeze_ins->getOperand(0));
    if (freeze_node == nullptr) {
      return false;
    }
    value_pegnode_map[freeze_ins] = freeze_node;
    return true;
  }

  return true;
}

bool ExpressionGraphPass::
translate_instruction(Instruction *ins,
                      PEG::PEGEdgeList *edge_list) {

  if (AllocaInst *alloca_ins = dyn_cast<AllocaInst>(ins)) {
    return true;
  }

  if (StoreInst *store_ins = dyn_cast<StoreInst>(ins)) {
    Value *store_value = translate_constantexpr(store_ins->getValueOperand(),
                                                edge_list);
    PEG::PEGNode *value_node = get_pegnode(store_value);
    Value *store_pointer = translate_constantexpr(store_ins->getPointerOperand(),
                                                  edge_list);
    PEG::PEGNode *pointer_node = get_pegnode(store_pointer);

    if (pointer_node == nullptr || value_node == nullptr) {
       OP << KRED << *store_ins <<
         " STORE has error for pegnode\n" << KNRM;
      return false;
    }

    edge_list->push_back(new PEG::PEGEdge(value_node, pointer_node,
                                          PEG::PEGEDGE_TYPE::STORE));
    return true;
  }

  if (LoadInst *load_ins = dyn_cast<LoadInst>(ins)) {
    Value *load_pointer = translate_constantexpr(load_ins->getPointerOperand(),
                                                 edge_list);
    PEG::PEGNode *pointer_node = get_pegnode(load_pointer);
    PEG::PEGNode *load_node = get_pegnode(load_ins);
    if (pointer_node == nullptr || load_node == nullptr) {
      OP << KRED << *load_ins
         <<" LOAD has error for pegnode\n" << KNRM;
      return false;
    }

    load_node->loc = pointer_node->loc;
    edge_list->push_back(new PEG::PEGEdge(pointer_node, load_node,
                                          PEG::PEGEDGE_TYPE::LOAD));
    return true;
  }

    if (AtomicRMWInst *rmw_ins = dyn_cast<AtomicRMWInst>(ins)) {
    Value *rmw_pointer = translate_constantexpr(rmw_ins->getPointerOperand(),
                                                edge_list);
    PEG::PEGNode *pointer_node = get_pegnode(rmw_pointer);
    PEG::PEGNode *rmw_node = get_pegnode(rmw_ins);
    if (pointer_node == nullptr || rmw_node == nullptr) {
      OP << KRED << *rmw_ins
         <<" Atomicrmwinst has error for pegnode\n" << KNRM;
      return false;
    }

    rmw_node->loc = pointer_node->loc;
    edge_list->push_back(new PEG::PEGEdge(pointer_node, rmw_node,
                                          PEG::PEGEDGE_TYPE::LOAD));
    return true;
  }


  if (GetElementPtrInst *gep_ins = dyn_cast<GetElementPtrInst>(ins)) {
    Value *pointer = translate_constantexpr(gep_ins->getPointerOperand(),
                                            edge_list);
    PEG::PEGNode *pointer_node = get_pegnode(pointer);
    if (pointer_node == nullptr) {
      OP << KRED << *gep_ins
         << " GEP has error for pegnode\n" << KNRM;
      return false;
    }

    if (!gep_ins->hasAllConstantIndices()) {
      return true;
    }

    PEG::PEGFieldNode *field_node =
      static_cast<PEG::PEGFieldNode *>(get_pegnode(gep_ins));
    field_node->update(gep_ins, &pointer_node->loc, pointer_node);
    edge_list->push_back(new PEG::PEGEdge(pointer_node, field_node,
                                          PEG::PEGEDGE_TYPE::FIELD));
    return true;
  }

  if (CastInst *cast_ins = dyn_cast<CastInst>(ins)) {
      translate_constantexpr(cast_ins->getOperand(0), edge_list);
      return true;
  }

  if (SelectInst *select_ins = dyn_cast<SelectInst>(ins)) {
    Value *true_value = translate_constantexpr(select_ins->getTrueValue(),
                                               edge_list);
    PEG::PEGNode *true_node = get_pegnode(true_value);
    Value *false_value = translate_constantexpr(select_ins->getFalseValue(),
                                                edge_list);
    PEG::PEGNode *false_node = get_pegnode(false_value);
    PEG::PEGNode *cond_node = get_pegnode(select_ins->getCondition());
    if (true_node == nullptr || false_node == nullptr ||
        cond_node == nullptr) {
      OP << KRED << *select_ins <<
        " SELECT has error for pegnode\n" << KNRM;
      return false;
    }

    PEG::PEGSelectNode *select_node =
      static_cast<PEG::PEGSelectNode *>(get_pegnode(select_ins));
    select_node->update(cond_node, true_node, false_node);
    edge_list->push_back(new PEG::PEGEdge(cond_node, select_node,
                                          PEG::PEGEDGE_TYPE::COND));
    edge_list->push_back(new PEG::PEGEdge(true_node, select_node,
                                          PEG::PEGEDGE_TYPE::ASSIGN));
    edge_list->push_back(new PEG::PEGEdge(false_node, select_node,
                                          PEG::PEGEDGE_TYPE::ASSIGN));
    return true;
  }

  if (ExtractValueInst *ev_ins = dyn_cast<ExtractValueInst>(ins)) {
    // actually we should handle extractvalue as gep,
    // but fuck it, it is to hard to compute offset
    // and it is too rare. So we simply return the agg.
    translate_constantexpr(ev_ins->getAggregateOperand(), edge_list);
    return true;
  }

  if (ExtractElementInst *ee_ins = dyn_cast<ExtractElementInst>(ins)) {
    translate_constantexpr(ee_ins->getVectorOperand(), edge_list);
    return true;
  }

  if (InsertElementInst *ie_ins = dyn_cast<InsertElementInst>(ins)) {
    Value *vector = translate_constantexpr(ie_ins->getOperand(0), edge_list);
    PEG::PEGNode *vector_node = get_pegnode(vector);
    Value *element = translate_constantexpr(ie_ins->getOperand(0), edge_list);
    PEG::PEGNode *element_node = get_pegnode(element);
    if (vector_node == nullptr || element_node == nullptr) {
      OP << KRED << *ie_ins
         << " INSERTELEMENT has error for pegnode\n" << KNRM;
      return false;
    }

    PEG::PEGNode *ie_node = get_pegnode(ie_ins);
    edge_list->push_back(new PEG::PEGEdge(vector_node, ie_node,
                                          PEG::PEGEDGE_TYPE::ASSIGN));
    edge_list->push_back(new PEG::PEGEdge(element_node, ie_node,
                                          PEG::PEGEDGE_TYPE::ASSIGN));
    return true;
  }

   if (InsertValueInst *iv_ins = dyn_cast<InsertValueInst>(ins)) {
    Value *vector = translate_constantexpr(iv_ins->getAggregateOperand(),
                                           edge_list);
    PEG::PEGNode *vector_node = get_pegnode(vector);
    Value *element = translate_constantexpr(iv_ins->getInsertedValueOperand(),
                                            edge_list);
    PEG::PEGNode *element_node = get_pegnode(element);
    if (vector_node == nullptr || element_node == nullptr) {
      OP << KRED << *iv_ins
         << " INSERTVALUE has error for pegnode\n" << KNRM;
      return false;
    }

    PEG::PEGNode *iv_node = get_pegnode(iv_ins);
    edge_list->push_back(new PEG::PEGEdge(vector_node, iv_node,
                                          PEG::PEGEDGE_TYPE::ASSIGN));
    edge_list->push_back(new PEG::PEGEdge(element_node, iv_node,
                                          PEG::PEGEDGE_TYPE::ASSIGN));
    return true;
  }

  if (CmpInst *cmp_ins = dyn_cast<CmpInst>(ins)) {
    Value *lhs = translate_constantexpr(cmp_ins->getOperand(0),
                                        edge_list);
    PEG::PEGNode *lhs_node = get_pegnode(lhs);
    Value *rhs = translate_constantexpr(cmp_ins->getOperand(1),
                                        edge_list);
    PEG::PEGNode *rhs_node = get_pegnode(rhs);
    if (lhs_node == nullptr || rhs_node == nullptr) {
      OP << KRED << *cmp_ins
         << " CMP has error for pegnode\n" << KNRM;
      return false;
    }

    PEG::PEGCmpNode *cmp_node =
      static_cast<PEG::PEGCmpNode *>(get_pegnode(cmp_ins));
    cmp_node->update(lhs_node, cmp_ins->getPredicate(), rhs_node);
    edge_list->push_back(new PEG::PEGEdge(lhs_node, cmp_node,
                                          PEG::PEGEDGE_TYPE::CMP));
    edge_list->push_back(new PEG::PEGEdge(rhs_node, cmp_node,
                                          PEG::PEGEDGE_TYPE::CMP));
    return true;
  }

  if (BinaryOperator *binary_ins = dyn_cast<BinaryOperator>(ins)) {
    PEG::PEGNode *node1 = get_pegnode(binary_ins->getOperand(0));
    PEG::PEGNode *binary_node = get_pegnode(binary_ins);
    if (node1 &&
        node1->type == PEG::PEGNODE_TYPE::CMP) {
      edge_list->push_back(new PEG::PEGEdge(node1, binary_node,
                                            PEG::PEGEDGE_TYPE::BINARY));
    }
    return true;
  }

  if (UnaryOperator *binary_ins = dyn_cast<UnaryOperator>(ins)) {
    return true;
  }

  // we do not handle them
  if (CallBrInst *callbr_ins = dyn_cast<CallBrInst>(ins)) {
    // this should be judged before callbase ==>
    // callbrinst  belongs to callbase
    return true;
  }

  if (CallBase *call_ins = dyn_cast<CallBase>(ins)) {
    PEG::PEGNodeList arg_nodes;
    for (auto &origin_arg : call_ins->args()) {
      if (isa<MetadataAsValue>(&origin_arg)) { // we do not collect metatdata now
        arg_nodes.push_back(PEG::pegnode_list[0]);
        continue;
      }
      Value *arg = translate_constantexpr(origin_arg,
                                          edge_list);
      PEG::PEGNode *arg_node = get_pegnode(arg);
      if (arg_node == nullptr) {
        OP << KRED << *call_ins
           << " CALL has error for pegnode\n" << KNRM;
        return false;
      }
      arg_nodes.push_back(arg_node);
    }

    PEG::PEGCallNode *call_node =
      static_cast<PEG::PEGCallNode *>(get_pegnode(call_ins));
    for (auto arg_node : arg_nodes) {
      edge_list->push_back(new PEG::PEGEdge(arg_node, call_node,
                                            PEG::PEGEDGE_TYPE::ARG));
    }
    PEG::PEGEdge *call_edge = new PEG::PEGEdge(call_node, call_node,
                                               PEG::PEGEDGE_TYPE::RESERVE);
    edge_list->push_back(call_edge);
    call_node->update(call_ins, &arg_nodes, call_edge);
    // do not consier invoke
    if (CallInst *tcall_ins = dyn_cast<CallInst>(ins))
      Ctx->callnode_funcs_map[call_node] = &Ctx->Callees[tcall_ins];
    return true;
  }

  if (PHINode *phi_ins = dyn_cast<PHINode>(ins)) {
    // PHI node should be handled in path construction

    PEG::PEGNodeList incoming_nodes;
    for (unsigned i = 0; i < phi_ins->getNumIncomingValues(); i++) {
      Value *incoming_value = translate_constantexpr(phi_ins->getIncomingValue(i),
                                                     edge_list);
      PEG::PEGNode *incoming_node = get_pegnode(incoming_value);
      if (incoming_node == nullptr) {
        OP << KRED << *phi_ins
           << " PHI has error for pegnode\n" << KNRM;
        return false;
      }
      incoming_nodes.push_back(incoming_node);
    }

    PEG::PEGPhiNode *phi_node =
      static_cast<PEG::PEGPhiNode *>(get_pegnode(phi_ins));
    phi_node->update(phi_ins, &incoming_nodes);
    for (auto incoming_node : incoming_nodes) {
      edge_list->push_back(new PEG::PEGEdge(incoming_node, phi_node,
                                            PEG::PEGEDGE_TYPE::PHI));
    }
    return true;
  }

  if (ShuffleVectorInst *sv_ins = dyn_cast<ShuffleVectorInst>(ins)) {
    Value *v1 = translate_constantexpr(sv_ins->getOperand(0), edge_list);
    PEG::PEGNode *v1_node = get_pegnode(v1);
    Value *v2 = translate_constantexpr(sv_ins->getOperand(1), edge_list);
    PEG::PEGNode *v2_node = get_pegnode(v2);
    PEG::PEGNode *sv_node = get_pegnode(sv_ins);
    if (v1_node)
      edge_list->push_back(new PEG::PEGEdge(v1_node, sv_node,
                                            PEG::PEGEDGE_TYPE::ASSIGN));
     if (v2_node)
      edge_list->push_back(new PEG::PEGEdge(v2_node, sv_node,
                                            PEG::PEGEDGE_TYPE::ASSIGN));
    return true;
  }


  if (BranchInst *br_ins = dyn_cast<BranchInst>(ins)) {
    if (br_ins->isUnconditional())
      return true;
    Value *condition = br_ins->getCondition();
    PEG::PEGNode *cond_node = get_pegnode(condition);
    if (cond_node == nullptr) {
      OP << KRED << *br_ins
         << " BRANCH has error for pegnode\n" << KNRM;
      return false;
    }

    PEG::PEGBrNode *br_node =
      static_cast<PEG::PEGBrNode *>(get_pegnode(br_ins));
    edge_list->push_back(new PEG::PEGEdge(cond_node, br_node,
                                          PEG::PEGEDGE_TYPE::COND));
    return true;
  }

  if (FreezeInst *freeze_ins = dyn_cast<FreezeInst>(ins)) {
    return true;
  }

  if (SwitchInst *switch_ins = dyn_cast<SwitchInst>(ins)) {
    Value *condition = switch_ins->getCondition();
    PEG::PEGNode *cond_node = get_pegnode(condition);
    if (cond_node == nullptr) {
      OP << KRED << *switch_ins
         << " SWITCH has error for pegnode\n" << KNRM;
      return false;
    }
    PEG::PEGNode *switch_node =get_pegnode(switch_ins);
    //br_node->update(br_ins, cond_node);
    edge_list->push_back(new PEG::PEGEdge(cond_node, switch_node,
                                          PEG::PEGEDGE_TYPE::COND));
    return true;
  }

  // we do not handle error handling system... for now
  if (LandingPadInst *lp_ins = dyn_cast<LandingPadInst>(ins)) {

    return true;
  }

  if (ResumeInst *lp_ins = dyn_cast<ResumeInst>(ins)) {
    return true;
  }

  if (UnreachableInst *un_ins = dyn_cast<UnreachableInst>(ins)) {
    return true;
  }

  if (FenceInst *fence_ins = dyn_cast<FenceInst>(ins)) {
    return true;
  }

  if (ReturnInst *ret_ins = dyn_cast<ReturnInst>(ins)) {
    if (!ret_ins->getReturnValue()) //return void. simply return true;
      return true;

    PEG::PEGNode *ret_node = get_pegnode(ret_ins);
    Value *retval = translate_constantexpr(ret_ins->getReturnValue(),
                                           edge_list);
    PEG::PEGNode *retval_node = get_pegnode(retval);
    if (retval_node == nullptr) {
      OP << KRED << *ret_ins
         << " RET has error for pegnode\n" << KNRM;
      return false;
    }

    edge_list->push_back(new PEG::PEGEdge(retval_node, ret_node,
                                          PEG::PEGEDGE_TYPE::ASSIGN));
    return true;
  }

  return false;
}

void ExpressionGraphPass::translate_basicblock(BasicBlock *bb) {
  PEG::PEGEdgeList *edge_list = &(Ctx->bb_edges_map[bb]);
  for (auto &ins : bb->instructionsWithoutDebug()) {
    if (!translate_instruction(&ins, edge_list)) {
      OP << KRED << ins << " unhandled" << KNRM << "\n";
    }
  }
}

void ExpressionGraphPass::translate_function(Function *f) {
  // skip debug function
  if (f &&
      f->getName().startswith("llvm."))
    return;

  for (auto& arg : f->args()) {
    PEG::PEGNode *node = PEG::create_pegnode(&arg);
    value_pegnode_map[&arg] = node;
    Ctx->arg_node_map[&arg] = node;
    node->top = true;
  }

  // create all the instruction node
  for (auto &bb : *f) {
    for (auto &ins : bb) {
      if (!create_instruction_node(&ins)) {
        OP << KRED << ins << " has error in node creating" << KNRM << "\n";
      }
    }
  }

  // map all the instruction without creating edge
  bool map_flag = false;
  while (!map_flag) {
    map_flag = true;
    for (auto map : constantexpr_ins_map) {
      bool tmp_flag = map_instruction(map.second);
      if (tmp_flag) {
        value_pegnode_map[map.first] = value_pegnode_map[map.second];
      }
      if (map_flag)
        map_flag = tmp_flag;
    }

    for (auto &bb : *f) {
      for (auto &ins : bb) {
        bool tmp_flag = map_instruction(&ins);
        if (map_flag)
          map_flag = tmp_flag;
      }
    }
  }

  // collecting edge
  for (auto& bb : *f) {
    translate_basicblock(&bb);
  }
}

void ExpressionGraphPass::add_global_value(llvm::Module *m) {
  // the 0 number of index should be NULL node.

  for (auto& global : m->globals()) {
    PEG::PEGNode *node = PEG::create_pegnode(&global);
    value_pegnode_map[&global] = node;
    node->top = true;
  }

  for (Function &curr_f : *m) {
    PEG::PEGNode *node = PEG::create_pegnode(&curr_f);
    value_pegnode_map[&curr_f] = node;
    node->top = true;
  }
}


bool ExpressionGraphPass::doModulePass(llvm::Module *M) {
  for (auto &func : *M) {
    translate_function(&func);
  }

  return false;
}

bool ExpressionGraphPass::doInitialization(llvm::Module* M) {
  datalayout = &(M->getDataLayout());
  add_global_value(M);
  return false;
}

bool ExpressionGraphPass::
doFinalization(__attribute__((unused)) llvm::Module* M) {
  return false;
}
