#ifndef PEGNODE_H
#define PEGNODE_H

#include <iostream>
#include <cassert>
#include <set>
#include <map>
#include <unordered_set>

#include "llvm/IR/Constants.h"
#include "llvm/IR/Instructions.h"

#include "Common.hh"

namespace PEG {
  class PEGNode;
  class PEGEdge;

  enum class PEGNODE_TYPE {
    STATIC = 0,
    POINTER,
    OBJECT,
    FIELD,
    CALL,
    CMP,
    PHI,
    SELECT,
    COND,
    INT,
    RET
  };
  // object means object and pointer. it is used in argument and global value.

  enum class PEGEDGE_TYPE {
    STORE,
    LOAD,
    FIELD,
    ASSIGN,
    PHI,
    CMP,
    COND,
    ARG,
    BINARY,
    RESERVE
  };

  struct location {
    enum TYPE {LOCAL = 0, GLOBAL, ARGUMENT};
    TYPE type;
    Function *loc_func;
    BasicBlock *loc_bb;
  };
  void set_location(Value *v, struct location *loc);

  class PEGNode {
  private:
    friend llvm::raw_ostream &operator<<(llvm::raw_ostream &, PEGNode &);
  public:
    int index;
    PEGNODE_TYPE type;
    bool partly;
    bool top;
    Value *value;
    struct location loc;
    PEGNode() {};
    PEGNode(PEGNODE_TYPE _t, int _index) :
      index(_index), type(_t), partly(false),
      top(false), value(nullptr) {};
  };

  // the copied peg node will not store to node list
  // and the index will be the same
  // If really want to add node to node list
  // use create instead.
  PEGNode *copy_node(PEGNode *);
  typedef std::vector<PEGNode *> PEGNodeList;
  typedef llvm::SmallPtrSet<PEG::PEGNode *, 8> SmallPEGNodeSet;
  typedef llvm::SmallPtrSet<PEG::PEGNode *, 32> LargePEGNodeSet;
  extern PEGNodeList pegnode_list;

  PEGNode *create_pegnode(Value *v);
  template <typename PEGNodeClass>
  PEGNodeClass *create_pegnode(Value *v) {
    int num = pegnode_list.size();
    PEGNodeClass *node = new PEGNodeClass(num);
    node->value = v;
    set_location(v, &node->loc);
    pegnode_list.push_back(node);
    return node;
  };

  class PEGFieldNode : public PEGNode {
  public:
    PEGNode *pointer;
    int64_t offset;
    PEGNODE_TYPE field_type;
    void update(GetElementPtrInst *gep_ins,
                struct location *loc,
                PEGNode *pointer);
    PEGFieldNode() {};
    PEGFieldNode(int64_t _offset, int _index, PEGNode *_pointer) :
      PEGNode(PEGNODE_TYPE::FIELD, _index), pointer(_pointer), offset(_offset) {};
    PEGFieldNode(int _index) :
      PEGNode(PEGNODE_TYPE::FIELD, _index) {};
  };

  class PEGCmpNode : public PEGNode {
  public:
    PEGNode *lhs;
    llvm::CmpInst::Predicate op;
    PEGNode *rhs;
    void update(PEGNode *lhs,
                llvm::CmpInst::Predicate op,
                PEGNode *rhs);
    PEGCmpNode() {};
    PEGCmpNode(PEGNode *_lhs,
               llvm::CmpInst::Predicate _op,
               PEGNode *_rhs, int _index) :
      PEGNode(PEGNODE_TYPE::CMP, _index), lhs(_lhs), op(_op), rhs(_rhs){}
    PEGCmpNode(int _index) : PEGNode(PEGNODE_TYPE::CMP, _index) {
      lhs = rhs = nullptr;
    };
  };

  class PEGCallNode : public PEGNode {
  public:
    bool indirect;
    StringRef callee;
    // we store pegcallnode -- functions to Ctx
    PEGNodeList args;
    PEGEdge *call_edge; // if recovery, store index instead of pointer
    void update(CallBase *call_ins,
                PEGNodeList *args,
                PEGEdge *call_edge);
    PEGCallNode() {};
    PEGCallNode(bool _indirect, StringRef _callee,
                PEGNodeList *_args, int _index) :
      PEGNode(PEGNODE_TYPE::CALL, _index),
      indirect(_indirect),
      callee(_callee) {
      this->args = *_args;
    }
    PEGCallNode(int _index) :
      PEGNode(PEGNODE_TYPE::CALL, _index) {};
  };

  class PEGPhiNode : public PEGNode {
  public:
    BasicBlock *curr_bb;
    llvm::DenseMap<BasicBlock *, PEGNode *> incomings;
    void update(PHINode *phi_ins, PEGNodeList *args);
    PEGPhiNode() {};
    PEGPhiNode(llvm::DenseMap<BasicBlock *, PEGNode *> *_incomings,
               BasicBlock* _curr_bb,
               int _index) :
      PEGNode(PEGNODE_TYPE::PHI, _index), curr_bb(_curr_bb) {
      this->incomings = *_incomings;
    }
    PEGPhiNode(int _index) :
      PEGNode(PEGNODE_TYPE::PHI, _index) {};
  };

  class PEGSelectNode : public PEGNode {
  public:
    PEGNode *cond;
    PEGNode *true_node;
    PEGNode *false_node;
    PEGSelectNode() {};
    PEGSelectNode(int _index) :
      PEGNode(PEGNODE_TYPE::SELECT, _index) {};
    void update(PEGNode *cond, PEGNode *true_node, PEGNode *false_node);
  };

  class PEGBrNode : public PEGNode {
  public:
    BasicBlock *true_bb;
    BasicBlock *false_bb;
    PEG::PEGNode *cond_node;
    void update(BranchInst *br_ins, PEGNode *cond_node);
    PEGBrNode() {};
    PEGBrNode(BasicBlock *_true_bb, BasicBlock *_false_bb, int _index) :
      PEGNode(PEGNODE_TYPE::COND, _index),
      true_bb(_true_bb),
      false_bb(_false_bb){}
    PEGBrNode(int _index) : PEGNode(PEGNODE_TYPE::COND, _index) {};
  };

  class PEGIntNode : public PEGNode {
  public:
    int int_val;
    PEGIntNode(int _int_val, int _index) :
      PEGNode(PEGNODE_TYPE::INT, _index), int_val(_int_val) {
      this->loc.type = location::GLOBAL;
    }
  };
  extern std::map<int64_t, PEGIntNode *> int_pegintnode_map;
  PEGIntNode *get_or_create_pegintnode(ConstantInt *v);

  extern llvm::DenseMap<Constant *, PEGNode *> constant_node_map;
  PEGNode *get_or_create_pegconstnode(Constant *v);

  class PEGEdge {
  private:
    friend llvm::raw_ostream &operator<<(llvm::raw_ostream &, PEGEdge &);
  public:
    PEGNode *src;
    PEGNode *dst;
    PEGEDGE_TYPE type;
    PEGEdge (PEGNode *_src, PEGNode *_dst, PEGEDGE_TYPE _type) :
      src(_src), dst(_dst), type(_type) {};
  };

  typedef llvm::SmallPtrSet<PEG::PEGEdge *, 8> SmallPEGEdgeSet;
  typedef std::list<PEGEdge *> PEGEdgeList;
  typedef std::map<PEG::PEGNode *, PEG::PEGEdgeList> NodeEdgesMap;

  enum class PEGVIRTUALEDGE_TYPE : int {
    LOAD,
    FIELD
  };

  class PEGVirtualEdge {
  public:
    PEGVIRTUALEDGE_TYPE type;
    int64_t offset;
    PEGVirtualEdge(PEGVIRTUALEDGE_TYPE _type, int64_t _offset):
      type(_type), offset(_offset){};
    PEGVirtualEdge(const PEGVirtualEdge &edge):
      type(edge.type), offset(edge.offset){};

    void operator=(const PEGVirtualEdge &edge) {
      this->type = edge.type;
      this->offset = edge.offset;
    };

    bool operator==(const PEGVirtualEdge &edge) const {
      return (this->type == edge.type &&
              this->offset == edge.offset);
    };

  };
  typedef std::list<PEGVirtualEdge> PEGVirtualEdgeList;

  class PEGVirtualNode {
  private:
    friend llvm::raw_ostream &operator<<(llvm::raw_ostream &, PEGVirtualNode &);
  public:
    PEGNode *pointer;
    PEGVirtualEdgeList path;
    PEGVirtualNode (PEGNode *_p):
      pointer(_p) {};

    PEGVirtualNode (PEGNode *_p, PEGVirtualEdgeList &_path):
      pointer(_p), path(_path){};

    PEGVirtualNode(const PEGVirtualNode &node) {
      pointer = node.pointer;
      path = node.path;
    }

    void operator=(const PEGVirtualNode &node) {
      pointer = node.pointer;
      path = node.path;
    }

    bool operator==(const PEGVirtualNode &node) const {
      return (this->pointer == node.pointer && this->path == node.path);
    }
  };

  typedef std::vector<PEGVirtualNode *> PEGVirtualNodesList;
  typedef llvm::DenseMap<PEGNode *, PEGVirtualNodesList *> VirtualNodesMap;
  typedef llvm::DenseSet<PEGVirtualNode *> PEGVirtualNodesSet;

  bool virtualnodesset_contains(PEGVirtualNodesSet *, PEGVirtualNode *);
  void init_peg_env();
}

#endif
