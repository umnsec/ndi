#include <list>
#include <set>
#include <llvm/IR/CFG.h>

#include "Tools.hh"
//Check if there is a path from fromBB to toBB
namespace Tools {

  bool from_bba_to_bbb(BasicBlock *a, BasicBlock *b){
    if(a == NULL || b == NULL)
      return false;

    //Use BFS to detect if there is a path from a to b
    std::list<BasicBlock *> EB; //BFS record list
    std::set<BasicBlock *> PB; //Global value set to avoid loop

    EB.push_back(a);
    while (!EB.empty()) {
      BasicBlock *TB = EB.front(); //Current checking block
      EB.pop_front();

      if (PB.find(TB) != PB.end())
        continue;
      PB.insert(TB);

      if(TB == b)
        return true;

      for(BasicBlock *Succ: successors(TB)){
        EB.push_back(Succ);
      }
    }
    return false;
  }

  bool from_bba_to_bbc_via_bbb(BasicBlock *a, BasicBlock *b,
                               BasicBlock *c) {
    if (from_bba_to_bbb(a, b))
      return from_bba_to_bbb(b, c);
    else
      return false;
  }

  bool from_bba_to_bbc_not_via_bbbs(BasicBlock *a, BasicBlockSet *b,
                                    BasicBlock *c) {
    // a and c should not be equal
    if(a == NULL || c == NULL)
      return false;

    //Use BFS to detect if there is a path from a to b
    std::list<BasicBlock *> bfs; //BFS record list
    std::set<BasicBlock *> identity; //Global value set to avoid loop

    bfs.push_back(a);
    while (!bfs.empty()) {
      BasicBlock *front = bfs.front(); //Current checking block
      bfs.pop_front();

      if (identity.find(front) != identity.end())
        continue;
      identity.insert(front);

      if(front == c)
        return true;

      for(BasicBlock *succ: successors(front)){
        if (!b->count(succ))
          bfs.push_back(succ);
      }
    }
    return false;
  }

  void find_ret_bb(BasicBlockSet *via_bbs, BasicBlockSet *ret_bbs) {
    for (auto bb : *via_bbs) {
      if (isa<ReturnInst>(bb->getTerminator())) {
        ret_bbs->insert(bb);
      }
    }
  }

  void source_analyze(Function *func, ValueSet *involved_set) {
    ValueSet iterate_set(*involved_set);
    ValueSet update_set;
    while (!iterate_set.empty()) {
      for (auto object : iterate_set) {
        for (User *user : object->users()) {
          if (Instruction *ins = dyn_cast<Instruction>(user)) {
            if (!involved_set->count(ins) &&
                ins->getParent() &&
                ins->getFunction() == func) {
              update_set.insert(ins);
              involved_set->insert(ins);
            }
          }
        }
      }
      iterate_set = update_set;
      update_set.clear();
    }
  }

  bool less_than_instructions(Instruction *ins1, Instruction *ins2) {
    // ins1 < ins2
    if (ins1 == NULL || ins2 == NULL)
      return false;
    if (ins1->getParent() == NULL || ins2->getParent() == NULL)
      return false;
    if (ins1->getParent() != ins2->getParent())
      return true;
    if (ins1 == ins2) {
      return false;
    }
    for (Instruction &tmp : *(ins1->getParent())) {
      if (&tmp == ins1) {
        return true;
      }
      if (&tmp == ins2) {
        return false;
      }
    }
    OP << KRED << "we shuld not reach here: tools:less_than_instructions\n";
    return false;
  }
}
