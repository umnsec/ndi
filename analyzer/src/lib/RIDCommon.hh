#ifndef RIDCOMMON_H
#define RIDCOMMON_H

#include <map>
#include <cstdint>
#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Function.h>

#include "Common.hh"
#include "PEGNode.hh"
using namespace llvm;

class Constraint {
private:
  friend llvm::raw_ostream &operator<<(llvm::raw_ostream &, const Constraint &);
public:
  PEG::PEGNode *lhs;
  PEG::SmallPEGNodeSet lhs_node;
  llvm::CmpInst::Predicate op;
  PEG::PEGNode *rhs;
  PEG::SmallPEGNodeSet rhs_node;
  Constraint() {};

  Constraint(PEG::PEGNode *_lhs,
             llvm::CmpInst::Predicate _op,
             PEG::PEGNode *_rhs): lhs(_lhs), op(_op), rhs(_rhs) {};

  Constraint(const Constraint &constraint) {
    this->lhs = constraint.lhs;
    this->lhs_node = constraint.lhs_node;
    this->op = constraint.op;
    this->rhs = constraint.rhs;
    this->rhs_node = constraint.rhs_node;
  };
};

class FunctionPath {
public:
  Function *func;
  llvm::SmallPtrSet<Argument *, 8> *args_set;
  llvm::DenseMap<Argument *, PEG::PEGNode *> *arg_node_map;
  std::map<PEG::PEGNode *, PEG::SmallPEGNodeSet> node_field_map;
  std::map<PEG::PEGNode *, PEG::SmallPEGNodeSet> point_to_value;
  std::map<PEG::PEGNode *, PEG::SmallPEGNodeSet> alias_value;
  PEG::PEGNode *ret;
  PEG::SmallPEGNodeSet retval;
  llvm::DenseMap<PEG::PEGNode *, PEG::PEGNode *> placeholder;
  PEG::SmallPEGNodeSet listdel_set;
  bool listdel_involved(PEG::SmallPEGNodeSet *);

  PEG::SmallPEGNodeSet kfree_set;
  PEG::SmallPEGNodeSet init_set;

  std::vector<BasicBlock *> path;
  llvm::SmallPtrSet<BasicBlock *, 16> path_set;
  llvm::DenseMap<BasicBlock *, PEG::PEGEdgeList *> bb_edges_map;
  llvm::SmallPtrSet<PEG::PEGEdge *, 16> phi_set;
  void update_phi_set();

  std::map<PEG::PEGCallNode *,
                 std::vector<PEG::SmallPEGNodeSet>> callee_args_map;
  std::map<PEG::PEGNode *, PEG::SmallPEGNodeSet> global_value_map;
  PEG::SmallPEGNodeSet global_involved_set;
  PEG::SmallPEGNodeSet maybe_undefined_set;
  std::map<BasicBlock *, bool> bb_direction;
  std::map<BasicBlock *, std::vector<Constraint>> constraints;

  std::map<PEG::PEGNode *, PEG::PEGVirtualNodesList> node_virtual_map;
  void find_virtualnode_backwords(PEG::PEGNode *node,
                                  PEG::PEGVirtualNodesList *virtual_nodes,
                                  PEG::PEGVirtualEdgeList *path,
                                  PEG::SmallPEGNodeSet *nodes_set,
                                  bool);
  PEG::PEGVirtualNodesList *get_virtualnodes(PEG::PEGNode *);

  FunctionPath(Function *f,
               llvm::DenseMap<Argument *, PEG::PEGNode *> *map,
               PEG::PEGNode *ret) {
    func = f;
    args_set = new llvm::SmallPtrSet<Argument *, 8>;
    arg_node_map = new llvm::DenseMap<Argument *, PEG::PEGNode *>;
    for (auto &arg : f->args()) {
      args_set->insert(&arg);
      PEG::PEGNode *node = (*map)[&arg];
      (*arg_node_map)[&arg] = node;
      node_field_map[node].insert(node);
    }
    this->ret = ret;
  };

  FunctionPath(FunctionPath& fp);
  void update_constraint_from_br(bool value,
                                 BranchInst *br_ins, LLVMContext &context);
  void remove_constraint_from_br(BranchInst *br_ins);
  bool contains_node(PEG::PEGNode *node);
  bool check_node(PEG::PEGNode *node);
  bool check_nodes(PEG::SmallPEGNodeSet *set);
  PEG::PEGEdgeList *collect_src_edge(PEG::PEGNode *node,
                                     PEG::PEGEdgeList *list);
  bool callee_involved(PEG::SmallPEGNodeSet *);
  bool find_point_to_set(PEG::PEGNode *,
                         PEG::SmallPEGNodeSet *,
                         bool);
  bool find_origin(PEG::PEGNode *, PEG::SmallPEGNodeSet *);
  void update_global_value();
  void print_path();

  // bool global_call(PEG::PEGCallNode *call_node);
  bool global_involved(PEG::SmallPEGNodeSet *);
  bool retval_involved(PEG::SmallPEGNodeSet *);
};

typedef llvm::SmallPtrSet<FunctionPath *, 8> FunctionPaths;

typedef std::pair<FunctionPath *, FunctionPath *> PathPair;

typedef llvm::DenseMap<Function *, FunctionPaths *> FunctionPathsMap;

class TargetFunctionState {
public:
  Function *func;
  std::set<PathPair> path_pairs;
  PEG::SmallPEGNodeSet cvs;
  PEG::SmallPEGNodeSet cvs_alias;
  PEG::SmallPEGNodeSet dists;
};

class EdgeInformation {
public:
  PEG::NodeEdgesMap src_map;
  PEG::NodeEdgesMap dst_map;
  llvm::DenseMap<PEG::PEGEdge *, BasicBlock *> edge_map;
};

#endif
