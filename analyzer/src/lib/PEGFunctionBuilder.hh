#ifndef PEGFUNCTIONBUILDER_H
#define PEGFUNCTIONBUILDER_H

#define KLIMIT 2
//#define KMAX 6
#define MAXFUNCTION 100000
#define MAXLISTLENGTH 10000000
// KMAX should be larger than KLIMIT
#define RELEASE_COLLECT 0
#include "Analyzer.hh"
#include "PEGNode.hh"

class FunctionPEGInform {
public:
  Function *func;
  FunctionPEGInform *parent;
  std::map<PEG::PEGNode *, PEG::PEGNode *> peg_node_map;
  llvm::DenseMap<Argument *, PEG::PEGNode *> arg_node_map;
  PEG::PEGNode *retnode;
  llvm::DenseMap<PEG::PEGCallNode *, FuncSet *> callnode_funcs_map;
  std::map<BasicBlock *, PEG::PEGEdgeList> bb_edges_map;
  std::map<PEG::PEGEdge *, BasicBlock *> edge_bb_map;
  std::map<PEG::PEGEdge *, std::vector<PEG::PEGEdge *>> links;
  PEG::SmallPEGEdgeSet dist_links;
  llvm::DenseMap<FunctionPEGInform *, PEG::PEGEdge *> inform_callee_map;
};

typedef llvm::SmallPtrSet<FunctionPEGInform *, 8> FunctionPEGInformSet;
typedef std::vector<FunctionPEGInform *> CallPath;

class PEGFunctionBuilder {
private:
  GlobalContext *ctx;
  Function *root_func;
  FunctionPEGInformSet root_inform;
  // only copy root_func
  // std::map<PEG::PEGNode *, PEG::SmallPEGNodeSet> same_nodes;

  std::map<Function *, unsigned> *callee_count;
  unsigned count_callees(Function *, FuncSet *);

  // PEG::SmallPEGNodeSet cv_alias;
  std::map<FunctionPEGInform *, PEG::SmallPEGNodeSet> root_cv_alias_map;
  //std::map<FunctionPEGInform *, PEG::SmallPEGEdgeSet> root_cv_uses_map;
  std::map<PEG::PEGNode *, PEG::SmallPEGNodeSet> dist_cache;
  PEG::SmallPEGNodeSet dist_alias;

  std::unordered_set<FunctionPEGInform *> func_peg_inform;
  std::unordered_set<FunctionPEGInform *> dist_peg_inform;
  FunctionPEGInform *build_func_peg_inform(Function *);

  PEG::PEGEdge *collect_related_args(FunctionPEGInform *,
                                     std::vector<PEG::PEGEdge *> *,
                                     PEG::PEGCallNode *);

  void update_caller_callee_inform(FunctionPEGInform *caller,
                                   PEG::PEGCallNode *call_node);
  std::map<Function *, FunctionPEGInform *> top_inform;
  FunctionPEGInform *add_links(Function *, FuncSet *, int);

  std::set<CallPath> dist_env;
  PEG::NodeEdgesMap src_map;
  PEG::NodeEdgesMap dst_map;
  llvm::DenseMap<PEG::PEGEdge *, FunctionPEGInform *> edge_map;

  void extend_field_nodes(PEG::SmallPEGNodeSet *,
                          PEG::SmallPEGNodeSet *);
  void build_one_inform(FunctionPEGInform *inform);
  void build_graph_inform();
  void build_call_path(FunctionPEGInform *inform,
                       CallPath *path);
  void collect_paths(CallPath *, CallPath *, PEG::SmallPEGEdgeSet *);
  void collect_call_path(FunctionPEGInform *inform,
                         PEG::SmallPEGEdgeSet *cv_uses);
  bool possible_judge(CallPath *path1, unsigned merge1,
                      CallPath *path2, unsigned merge2,
                      PEG::SmallPEGEdgeSet *cv_uses);
  void merge_two_path(CallPath *path1, unsigned merge1,
                      CallPath *path2, unsigned merge2,
                      CallPath *merge_path);
  void add_dist_env(CallPath *path, unsigned stop);
  void rebuild_graph_inform();
  void update_graph_inform(FunctionPEGInform *);
  void add_src_related_nodes(PEG::PEGVirtualNode *,
                             std::list<PEG::PEGVirtualNode> *,
                             PEG::SmallPEGNodeSet *, bool);
  void add_dst_related_nodes(PEG::PEGVirtualNode *,
                             std::list<PEG::PEGVirtualNode> *,
                             PEG::SmallPEGNodeSet *, bool);
  bool edge_lt(PEG::PEGEdge *, PEG::PEGEdge *, FunctionPEGInform *);
  void identify_cmp_ret(PEG::PEGNode *cmp_node);
  void identify_lal(FunctionPEGInform * inform, PEG::PEGNode *load_node);
  bool build_dist_env_recursively(FunctionPEGInform *, PEG::PEGEdge *,
                                  CallPath *, int, PEG::SmallPEGEdgeSet *);
public:
  std::map<FunctionPEGInform *, PEG::SmallPEGEdgeSet> root_cv_uses_map;
  PEG::SmallPEGEdgeSet cv_uses;
  PEG::SmallPEGEdgeSet dist_uses;

  PEGFunctionBuilder(GlobalContext *_Ctx, Function *_f,
                     std::map<Function *, unsigned> *_callee_count)
    : ctx(_Ctx), root_func(_f), callee_count(_callee_count){};
  ~PEGFunctionBuilder();

  int build_env();
  void print_node_inform(PEG::PEGNode *);
  int identify_cv_alias(PEG::PEGNode *);
  void identify_cv_use(bool add);
  int build_dist_env();
  void add_node_to_cache(PEG::PEGVirtualNode *,
                         std::list<PEG::PEGVirtualNode> *);
  void identify_dist_without_cache(PEG::PEGNode *);
  void identify_dist_use();
  bool judge();
};

#endif
